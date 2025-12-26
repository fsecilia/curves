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

auto calc_t(s64 x, int width_log2) -> s64 { return ::calc_t(x, width_log2); }

auto locate_segment(s64 x, s64* segment_index, s64* t) noexcept -> void {
  return ::locate_segment(x, segment_index, t);
}

auto transform_v_to_x(const struct curves_spline* spline, s64 v) noexcept
    -> s64 {
  return ::transform_v_to_x(spline, v);
}

auto eval(const struct curves_spline* spline, s64 v) noexcept -> s64 {
  return ::curves_spline_eval(spline, v);
}

}  // namespace curves::spline
