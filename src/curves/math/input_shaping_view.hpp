// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Floating-point wrapper for kernel shaping evaluation.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/input_shaping.hpp>

namespace curves {

struct ShapingResult {
  real_t u;    // U(v)
  real_t du;   // U'(v)
  real_t d2u;  // U''(v)
};

/*!
  InputShapingView

  Non-owning view of kernel shaping parameters in floating-point. Implemented
  in terms of the actual kernel shaping fixed-point code for 1:1 evaluation.
*/
class InputShapingView {
 public:
  explicit InputShapingView(const curves_shaping_params& params) noexcept
      : params_{&params} {}

  InputShapingView() noexcept = default;

  auto valid() const noexcept -> bool { return params_ != nullptr; }

  /*
    Apply shaping to raw velocity v.

    Input:  v in counts/ms, in floating-point
    Output: u in counts/ms, in floating-point

    This calls the kernel's fixed-point code and converts back.
  */
  auto eval(real_t v) const noexcept -> real_t {
    if (!valid()) return v;

    s64 v_fixed = real_to_fixed(v);
    s64 u_fixed = curves_shaping_apply(v_fixed, params_);
    return fixed_to_real(u_fixed);
  }

  /*
   * Evaluate U(v), U'(v), U''(v).
   *
   * U(v) uses kernel code for exact match.
   * Derivatives computed in floating-point.
   */
  auto operator()(real_t v) const noexcept -> ShapingResult {
    if (!params_) return {v, real_t{1}, real_t{0}};

    // Get parameters in floating-point.
    const auto v_begin_in = fixed_to_real(params_->ease_in.transition.v_begin);
    const auto v_width_in = fixed_to_real(params_->ease_in.transition.v_width);
    const auto v_width_in_inv =
        fixed_to_real(params_->ease_in.transition.v_width_inv);

    const auto v_begin_out =
        fixed_to_real(params_->ease_out.transition.v_begin);
    const auto v_width_out =
        fixed_to_real(params_->ease_out.transition.v_width);
    const auto v_width_out_inv =
        fixed_to_real(params_->ease_out.transition.v_width_inv);

    // Evaluate U(v) via kernel for exact match.
    const auto u = eval(v);

    // Determine region and compute derivatives.

    // Stage 1: Ease-in
    const auto v_end_in = v_begin_in + v_width_in;
    if (v_width_in > 0 && v < v_begin_in) {
      // Floor segment.
      return {u, real_t{0}, real_t{0}};
    }

    if (v_width_in > 0 && v < v_end_in) {
      // Transition segment.
      const auto t = (v - v_begin_in) * v_width_in_inv;
      const auto [dP, d2P] = poly_derivatives(t);
      const auto du = dP;
      const auto d2u = d2P * v_width_in_inv;
      return {u, du, d2u};
    }

    // Stage 2: Ease-out
    const auto v_end_out = v_begin_out + v_width_out;
    if (v < v_begin_out) {
      // Linear segment.
      return {u, real_t{1}, real_t{0}};
    }

    if (v < v_end_out) {
      // Transition segment.
      const auto t = (v - v_begin_out) * v_width_out_inv;
      const auto [dP, d2P] = poly_derivatives(t);
      const auto du = 1 - dP;
      const auto d2u = -d2P * v_width_out_inv;
      return {u, du, d2u};
    }

    // Ceiling segment.
    return {u, real_t{0}, real_t{0}};
  }

  // Ease-in parameters.
  auto ease_in_u_floor() const noexcept -> real_t {
    return valid() ? fixed_to_real(params_->ease_in.u_floor) : 0.0;
  }
  auto ease_in_transition_v_begin() const noexcept -> real_t {
    return valid() ? fixed_to_real(params_->ease_in.transition.v_begin) : 0.0;
  }
  auto ease_in_transition_v_width() const noexcept -> real_t {
    return valid() ? fixed_to_real(params_->ease_in.transition.v_width) : 0.0;
  }
  auto ease_in_u_lag() const noexcept -> real_t {
    return valid() ? fixed_to_real(params_->ease_in.u_lag) : 0.0;
  }

  // Ease-out parameters.
  auto ease_out_transition_v_begin() const noexcept -> real_t {
    return valid() ? fixed_to_real(params_->ease_out.transition.v_begin) : 0.0;
  }
  auto ease_out_transition_v_width() const noexcept -> real_t {
    return valid() ? fixed_to_real(params_->ease_out.transition.v_width) : 0.0;
  }
  auto ease_out_u_ceiling() const noexcept -> real_t {
    return valid() ? fixed_to_real(params_->ease_out.u_ceiling) : 0.0;
  }

  // Derived boundaries.
  auto ease_in_transition_v_end() const noexcept -> real_t {
    return ease_in_transition_v_begin() + ease_in_transition_v_width();
  }
  auto ease_out_transition_v_end() const noexcept -> real_t {
    return ease_out_transition_v_begin() + ease_out_transition_v_width();
  }

 private:
  const curves_shaping_params* params_;

  /*
   * Compute P'(t) and P''(t) for the transition polynomial.
   *
   * P(t)   = t^6 - 3t^5 + 2.5t^4
   * P'(t)  = 6t^5 - 15t^4 + 10t^3
   * P''(t) = 30t^4 - 60t^3 + 30t^2
   */
  struct PolyDerivatives {
    real_t dP;
    real_t d2P;
  };

  static auto poly_derivatives(real_t t) noexcept -> PolyDerivatives {
    auto t2 = t * t;
    auto t3 = t2 * t;
    auto t4 = t2 * t2;
    auto t5 = t4 * t;

    auto dP = 6 * t5 - 15 * t4 + 10 * t3;
    auto d2P = 30 * t4 - 60 * t3 + 30 * t2;

    return {dP, d2P};
  }
};

}  // namespace curves
