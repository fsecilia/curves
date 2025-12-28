// SPDX-License-Identifier: MIT
/*!
  \file
  \brief C++ wrappers for kernel-mode c functions.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "spline.hpp"

extern "C" {
#include <curves/driver/spline.c>
}  // extern "C"

namespace curves::spline {

auto map_x_to_t(s64 x, int width_log2) -> s64 {
  return ::map_x_to_t(x, width_log2);
}

auto map_v_to_x(const struct curves_spline* spline, s64 v) noexcept -> s64 {
  return ::map_v_to_x(spline, v);
}

auto resolve_x(s64 x) noexcept -> curves_spline_coords {
  return ::resolve_x(x);
}

auto eval(const struct curves_spline_segment* segment, s64 t) noexcept -> s64 {
  return ::eval_segment(segment, t);
}

auto eval(const struct curves_spline* spline, s64 v) noexcept -> s64 {
  return ::curves_spline_eval(spline, v);
}

}  // namespace curves::spline
