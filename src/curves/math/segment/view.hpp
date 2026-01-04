// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Floating-point view wrapper for segment evaluation.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/math/fixed.hpp>
#include <curves/math/segment/segment.hpp>

#include <cstdint>

namespace curves::segment {

/*!
  Non-owning view for evaluating a normalized segment in floating-point.

  This wrapper converts between floating-point and the kernel's fixed-point
  evaluation, allowing the frontend to preview results that match the kernel
  exactly (within floating-point conversion precision).
*/
class SegmentView {
 public:
  /// Constructs a view of the given segment. The segment must outlive the view.
  explicit SegmentView(const NormalizedSegment& segment) noexcept
      : segment_{&segment} {}

  /// Returns the inverse width as a floating-point value.
  auto inv_width() const noexcept -> real_t {
    return to_real(segment_->inv_width.value, segment_->inv_width.shift);
  }

  /// Converts spline x to segment-local t in [0, 1].
  auto x_to_t(real_t x, real_t x0) const noexcept -> real_t {
    const auto x_fixed = to_fixed(x, kInputFracBits);
    const auto x0_fixed = to_fixed(x0, kInputFracBits);
    const auto t_fixed =
        segment::x_to_t(segment_->inv_width, x_fixed, x0_fixed, kInputFracBits);
    return to_real(t_fixed, kTFracBits);
  }

  /// Evaluates the polynomial at t using the kernel's Horner's method.
  auto eval(real_t t) const noexcept -> real_t {
    const auto t_fixed = to_fixed<uint64_t>(t, kTFracBits);
    const auto result = eval_poly(segment_->poly, t_fixed);
    return to_real(result, kOutputFracBits);
  }

  /// Convenience operator for evaluation.
  auto operator()(real_t t) const noexcept -> real_t { return eval(t); }

 private:
  const NormalizedSegment* segment_;
};

}  // namespace curves::segment
