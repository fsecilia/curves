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

struct InputShapingConfig {
  // Stage 1: Ease-in
  real_t floor_v_width = 0.0;  // How long floor lasts (v units)
  real_t ease_in_width = 0.0;  // Transition width (v units)

  // Stage 2: Ease-out
  real_t ease_out_v_begin = 100;  // Where ceiling transition starts
  real_t ease_out_width = 10;     // Ceiling transition width
};

// Solver becomes trivial - just geometry
inline auto solve_input_shaping(const InputShapingConfig& config)
    -> curves_shaping_params {
  curves_shaping_params p{};

  // Stage 1: Ease-in
  // Floor is at u = 0 always
  p.ease_in.u_floor = 0;

  // Transition starts at end of floor
  const auto v_in_begin = config.floor_v_width;
  const auto v_in_width = config.ease_in_width;
  p.ease_in.transition.v_begin = real_to_fixed(v_in_begin);
  p.ease_in.transition.v_width = real_to_fixed(v_in_width);
  p.ease_in.transition.v_width_inv =
      v_in_width ? real_to_fixed(1.0L / v_in_width) : 0LL;

  // Lag for continuity: at v_end, transition outputs v_width * 0.5
  // Linear outputs v - lag, so lag = v_end - (0 + v_width * 0.5)
  const auto v_end = v_in_begin + v_in_width;
  const auto transition_height = v_in_width * 0.5;  // kEasePoly.area_ratio()
  const auto u_lag = v_end - transition_height;
  p.ease_in.u_lag = real_to_fixed(u_lag);

  // Stage 2: Ease-out (same as before, no changes needed)
  const auto v_out_width = config.ease_out_width;
  p.ease_out.transition.v_begin = real_to_fixed(config.ease_out_v_begin);
  p.ease_out.transition.v_width = real_to_fixed(v_out_width);
  p.ease_out.transition.v_width_inv =
      v_out_width ? real_to_fixed(1.0 / v_out_width) : 0LL;
  p.ease_out.u_ceiling =
      real_to_fixed(config.ease_out_v_begin + v_out_width * 0.5);

  return p;
}

}  // namespace curves
