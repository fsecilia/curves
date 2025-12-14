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

namespace curves {

auto locate_knot(int knot) noexcept -> s64 { return ::locate_knot(knot); }

auto locate_segment(s64 x, s64* segment_index, s64* t) noexcept -> void {
  return ::locate_segment(x, segment_index, t);
}

auto eval(const struct curves_spline* spline, s64 x) noexcept -> s64 {
  return ::curves_spline_eval(spline, x);
}

}  // namespace curves
