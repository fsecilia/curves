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
  for (int i = 0; i < 4; ++i) dst.v[i] = src.coeffs[i] & ~CURVES_PAYLOAD_MASK;

  // Scatter inverse width.
  constexpr auto w_v2_bits = CURVES_PAYLOAD_BITS - (CURVES_SHIFT_BITS * 2);
  constexpr auto w_v3_bits = w_v2_bits - CURVES_SHIFT_BITS;
  constexpr auto w_v2_mask = (1ULL << w_v2_bits) - 1;
  constexpr auto w_v3_mask = (1ULL << w_v3_bits) - 1;
  constexpr auto offset_v2 = CURVES_PAYLOAD_BITS * 2;
  constexpr auto offset_v3 = offset_v2 + w_v2_bits;
  dst.v[0] |= src.inv_width & CURVES_PAYLOAD_MASK;
  dst.v[1] |= (src.inv_width >> CURVES_PAYLOAD_BITS) & CURVES_PAYLOAD_MASK;
  dst.v[2] |= ((src.inv_width >> offset_v2) & w_v2_mask)
              << (CURVES_SHIFT_BITS * 2);
  dst.v[3] |= ((src.inv_width >> offset_v3) & w_v3_mask)
              << (CURVES_SHIFT_BITS * 3);

  // Shifts.
  dst.v[2] |= (src.relative_shifts[0] & CURVES_SHIFT_MASK);
  dst.v[2] |= (src.relative_shifts[1] & CURVES_SHIFT_MASK) << CURVES_SHIFT_BITS;
  dst.v[3] |= (src.relative_shifts[2] & CURVES_SHIFT_MASK);
  dst.v[3] |= (src.relative_shifts[3] & CURVES_SHIFT_MASK) << CURVES_SHIFT_BITS;
  dst.v[3] |= (src.inv_width_shift & CURVES_SHIFT_MASK)
              << (CURVES_SHIFT_BITS * 2);

  return dst;
}

}  // namespace curves
