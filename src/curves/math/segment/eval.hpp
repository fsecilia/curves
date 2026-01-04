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
#include <curves/driver/segment/eval.h>
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
    /*
      This should use __curves_segment_x_to_t, but we still have to determine
      how many frac bits it will use. Until the pipeline is fleshed out more,
      just use float.
    */
    return (x - x0) * inv_width();
  }

  auto operator()(real_t t) const noexcept -> real_t { return eval(t); }
  auto eval(real_t t) const noexcept -> real_t {
    if (!valid()) return 0.0L;

    return __curves_segment_eval_poly(&segment_->poly,
                                      to_fixed(t, CURVES_SEGMENT_T_FRAC_BITS));
  }

 private:
  const curves_normalized_segment* segment_{};
};

}  // namespace curves
