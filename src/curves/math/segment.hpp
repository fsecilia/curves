// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Floating-point wrapper for kernel spline segments.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>

extern "C" {
#include <curves/driver/segment.h>
}  // extern "C"

namespace curves {

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
