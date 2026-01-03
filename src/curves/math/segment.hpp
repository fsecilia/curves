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
#include <curves/driver/segment.h>
}  // extern "C"

namespace curves {

class SegmentView {
 public:
  SegmentView(const curves_normalized_segment* segment = nullptr) noexcept
      : segment_{segment} {}

  auto valid() const noexcept -> bool { return segment_; }

  auto inv_width() const noexcept -> real_t {
    if (!valid()) return 0.0L;

    return to_real(segment_->inv_width, segment_->inv_width_shift);
  }

  auto x_to_t(real_t x, real_t x0) const noexcept -> real_t {
    return (x - x0) * inv_width();
  }

  auto operator()(real_t t) const noexcept -> real_t { return eval(t); }
  auto eval(real_t t) const noexcept -> real_t {
    if (!valid()) return 0.0L;

    return curves_eval_segment(segment_, to_fixed(t, 64));
  }

 private:
  const curves_normalized_segment* segment_{};
};

auto curves_pack_segment(const curves_normalized_segment& src) noexcept
    -> curves_packed_segment {
  auto dst = curves_packed_segment{};

  // Coefficients.
  for (int i = 0; i < 4; ++i) {
    dst.v[i] = src.coeffs[i] << CURVES_SEGMENT_COEFFICIENT_SHIFT;
  }

  // Scatter inverse width.
  const auto inv_width = src.inv_width & CURVES_SEGMENT_MASK;
  dst.v[0] |= inv_width & CURVES_SEGMENT_PAYLOAD_MASK;
  dst.v[1] |=
      (inv_width >> CURVES_SEGMENT_PAYLOAD_BITS) & CURVES_SEGMENT_PAYLOAD_MASK;
  dst.v[2] |= (inv_width >> 2 * CURVES_SEGMENT_PAYLOAD_BITS)
              << (CURVES_SEGMENT_PAYLOAD_FIELD_BITS * 2);

  // Shifts.
  dst.v[2] |= (src.relative_shifts[0] & CURVES_SEGMENT_PAYLOAD_FIELD_MASK);
  dst.v[2] |= (src.inv_width_shift & CURVES_SEGMENT_PAYLOAD_FIELD_MASK)
              << CURVES_SEGMENT_PAYLOAD_FIELD_BITS;
  dst.v[3] |= (src.relative_shifts[1] & CURVES_SEGMENT_PAYLOAD_FIELD_MASK);
  dst.v[3] |= (src.relative_shifts[2] & CURVES_SEGMENT_PAYLOAD_FIELD_MASK)
              << CURVES_SEGMENT_PAYLOAD_FIELD_BITS;
  dst.v[3] |= (src.relative_shifts[3] & CURVES_SEGMENT_PAYLOAD_TOP_MASK)
              << (CURVES_SEGMENT_PAYLOAD_FIELD_BITS * 2);

  return dst;
}

}  // namespace curves
