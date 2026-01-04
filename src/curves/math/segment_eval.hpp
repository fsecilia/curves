// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Floating-point wrapper for kernel spline segments.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/fixed.hpp>

extern "C" {
#include <curves/driver/segment_eval.h>
}  // extern "C"

namespace curves {

class SegmentView {
 public:
  SegmentView(const curves_normalized_segment* segment = nullptr) noexcept
      : segment_{segment} {}

  auto valid() const noexcept -> bool { return segment_; }

  auto inv_width() const noexcept -> real_t {
    if (!valid()) return 0.0L;

    return to_real(segment_->inv_width.value, segment_->inv_width.shift);
  }

  auto x_to_t(real_t x, real_t x0) const noexcept -> real_t {
    const auto x_fixed = to_fixed(x, CURVES_SEGMENT_IN_FRAC_BITS);
    const auto x0_fixed = to_fixed(x0, CURVES_SEGMENT_IN_FRAC_BITS);
    const auto t_fixed = __curves_segment_x_to_t(
        &segment_->inv_width, x_fixed, x0_fixed, CURVES_SEGMENT_IN_FRAC_BITS);
    return to_real(t_fixed, CURVES_SEGMENT_T_FRAC_BITS);
  }

  auto operator()(real_t t) const noexcept -> real_t { return eval(t); }
  auto eval(real_t t) const noexcept -> real_t {
    if (!valid()) return 0.0L;

    const auto t_fixed = to_fixed<u64>(t, CURVES_SEGMENT_T_FRAC_BITS);
    const auto result_fixed =
        __curves_segment_eval_poly(&segment_->poly, t_fixed);
    return to_real(result_fixed, CURVES_SEGMENT_OUT_FRAC_BITS);
  }

 private:
  const curves_normalized_segment* segment_{};
};

}  // namespace curves
