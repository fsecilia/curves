// SPDX-License-Identifier: MIT
/*!
  \file
  \brief User mode additions to the kernel input shaping module.

  This module implements solving and construction of shaping curve.
  \sa input_shaping.h for definitions and evaluation.

  Config vs State
  ---------------
  Below, you'll see config struct and state structs. Config is what we present
  to the user, in domains and units most useful to the user. State is how those
  are translated into domains and units most useful to the driver.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

extern "C" {
#include <curves/driver/input_shaping.h>
}  // extern "C"

#include <curves/lib.hpp>
#include <curves/config/curve.hpp>
#include <curves/math/fixed.hpp>
#include <curves/math/inverse_function.hpp>
#include <curves/math/spline.hpp>
#include <utility>

namespace curves {

// ----------------------------------------------------------------------------
// Transition Polynomial
// ----------------------------------------------------------------------------

/*
  P(t) = t^4 * (2.5 - 3t + t^2) = 2.5t^4 - 3t^5 + t^6

  This is the integral of smootherstep applied to slope. Properties:

    P(0) = 0      P(1) = 0.5     (area ratio)
    P'(0) = 0     P'(1) = 1      (slope continuity)
    P''(0) = 0    P''(1) = 0     (curvature continuity)
    P'''(0) = 0   P'''(1) = 0    (jerk continuity)

  This gives C³ continuity when concatenating floor/transition/linear.
  The felt gain (which is what your hand experiences) has continuous jerk.
*/
struct EasePolynomial {
  real_t c4;
  real_t c5;
  real_t c6;

  constexpr auto area_ratio() const -> real_t { return c4 + c5 + c6; }

  constexpr auto operator()(real_t t) const -> real_t {
    const auto t2 = t * t;
    const auto t4 = t2 * t2;
    return t4 * (c4 + t * c5 + t2 * c6);
  }
};

// Integral of smootherstep. C^3 continuous, area ratio 0.5.
inline constexpr auto kEasePoly = EasePolynomial{2.5, -3.0, 1.0};

//! Domain covered by a transition.
struct ShapingTransition {
  real_t v_begin;  //!< Beginning of transition, velocity.
  real_t v_width;  //!< Width of transition, velocity.
};

// ----------------------------------------------------------------------------
// Stage 1: Ease-In Configuration and State
// ----------------------------------------------------------------------------

//! Config, as specified by ui.
struct EaseInConfig {
  static auto constexpr y_floor_target_default = real_t{0};

  // Floor level in user's chosen display space, sensitivity or gain.
  real_t y_floor_target = y_floor_target_default;

  static auto constexpr v_width_default = real_t{0};
  static auto constexpr v_begin_default = real_t{0};
  ShapingTransition transition{v_width_default, v_begin_default};
};

//! State, solved for kernel params.
struct EaseInState {
  real_t u_floor = 0.0;
  real_t v_width_inv = 0.0;
  real_t u_lag = 0.0;
};

// ----------------------------------------------------------------------------
// Stage 2: Ease-Out Configuration and State
// ----------------------------------------------------------------------------

struct EaseOutConfig {
  static auto constexpr begin_default_scale = real_t{0.1};
  static auto constexpr width_default_scale = real_t{0.8};
  ShapingTransition transition;
};

struct EaseOutState {
  real_t v_width_inv = 0.0;
  real_t u_ceiling = 0.0;
};

// ----------------------------------------------------------------------------
// Combined Configuration
// ----------------------------------------------------------------------------

struct InputShapingConfig {
  EaseInConfig ease_in;
  EaseOutConfig ease_out;
};

struct InputShapingState {
  EaseInState ease_in;
  EaseOutState ease_out;
};

// ----------------------------------------------------------------------------
// Stage 1 Solver
// ----------------------------------------------------------------------------

struct SolveEaseIn {
  // DisplayCurve: callable as real_t(real_t u), returns S or G at that u.
  template <typename DisplayCurve>
  EaseInState operator()(const EaseInConfig& config,
                         DisplayCurve&& display_curve, real_t u_max) const {
    EaseInState state{};

    // Find u_floor by inverting the display curve.
    if (config.y_floor_target <= 0.0) {
      state.u_floor = 0.0;
    } else {
      state.u_floor =
          inverse_via_partition(display_curve, config.y_floor_target, u_max);
    }

    // Compute reciprocal width for division in eval.
    const auto v_width = std::max(0.0L, config.transition.v_width);
    state.v_width_inv = (v_width > 0.0) ? (1.0 / v_width) : 0.0;

    /*
      Compute lag for continuity at transition end.

      At v = v_begin + v_width:
        Transition output: u_floor + v_width * 0.5
        Linear output:     v - u_lag

      Setting equal:
        u_lag = (v_begin + v_width) - u_floor - v_width * 0.5
              = v_begin + v_width * 0.5 - u_floor
    */
    const auto v_end = config.transition.v_begin + v_width;
    const auto transition_height = v_width * kEasePoly.area_ratio();
    state.u_lag = v_end - (state.u_floor + transition_height);

    return state;
  }
};
inline constexpr SolveEaseIn solve_ease_in{};

// ----------------------------------------------------------------------------
// Stage 2 Solver
// ----------------------------------------------------------------------------

struct SolveEaseOut {
  EaseOutState operator()(const EaseOutConfig& config) const {
    EaseOutState state{};

    const auto v_width = std::max(0.0L, config.transition.v_width);
    state.v_width_inv = (v_width > 0.0) ? (1.0 / v_width) : 0.0;
    state.u_ceiling =
        config.transition.v_begin + v_width * kEasePoly.area_ratio();

    return state;
  }
};
inline constexpr SolveEaseOut solve_ease_out{};

// ----------------------------------------------------------------------------
// Combined Solver
// ----------------------------------------------------------------------------

struct SolveInputShaping {
  template <typename DisplayCurve>
  InputShapingState operator()(const InputShapingConfig& config,
                               DisplayCurve&& display_curve,
                               real_t u_max) const {
    return InputShapingState{
        .ease_in = solve_ease_in(config.ease_in, display_curve, u_max),
        .ease_out = solve_ease_out(config.ease_out),
    };
  }
};
inline constexpr SolveInputShaping solve_input_shaping{};

// ----------------------------------------------------------------------------
// Kernel Parameter Builder
// ----------------------------------------------------------------------------

struct BuildKernelParams {
  curves_shaping_params operator()(const EaseInConfig& ease_in_cfg,
                                   const EaseInState& ease_in_state,
                                   const EaseOutConfig& ease_out_cfg,
                                   const EaseOutState& ease_out_state) const {
    curves_shaping_params p;

    // Stage 1: Ease-in
    p.ease_in = curves_shaping_ease_in{
        .u_floor = to_fixed(ease_in_state.u_floor),
        .u_lag = to_fixed(ease_in_state.u_lag),
        .transition =
            curves_shaping_transition{
                .v_begin = to_fixed(ease_in_cfg.transition.v_begin),
                .v_width = to_fixed(ease_in_cfg.transition.v_width),
                .v_width_inv = to_fixed(ease_in_state.v_width_inv),
            },
    };

    // Stage 2: Ease-out
    p.ease_out = curves_shaping_ease_out{
        .u_ceiling = to_fixed(ease_out_state.u_ceiling),
        .transition =
            curves_shaping_transition{
                .v_begin = to_fixed(ease_out_cfg.transition.v_begin),
                .v_width = to_fixed(ease_out_cfg.transition.v_width),
                .v_width_inv = to_fixed(ease_out_state.v_width_inv),
            },
    };

    return p;
  }

  curves_shaping_params operator()(const InputShapingConfig& config,
                                   const InputShapingState& state) const {
    return (*this)(config.ease_in, state.ease_in, config.ease_out,
                   state.ease_out);
  }

 private:
  static auto to_fixed(real_t val) noexcept -> s64 { return Fixed{val}.raw; }
};

inline constexpr BuildKernelParams build_kernel_params{};

// ----------------------------------------------------------------------------
// Default Configurations
// ----------------------------------------------------------------------------

inline EaseInConfig default_ease_in_config() { return EaseInConfig{}; }

inline auto default_ease_out_config(real_t v_end) noexcept -> EaseOutConfig {
  return EaseOutConfig{
      .transition = {.v_begin = v_end * EaseOutConfig::begin_default_scale,
                     .v_width = v_end * EaseOutConfig::width_default_scale}};
}

inline auto default_shaping_config(real_t v_end) noexcept
    -> InputShapingConfig {
  return InputShapingConfig{.ease_in = default_ease_in_config(),
                            .ease_out = default_ease_out_config(v_end)};
}

}  // namespace curves
