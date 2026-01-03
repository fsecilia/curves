// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Floating-point segment packing for kernel spline segments.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/fixed.hpp>
#include <curves/math/segment_eval.hpp>

extern "C" {
#include <curves/driver/segment_unpacking.h>
}  // extern "C"

namespace curves {

auto curves_pack_segment(const curves_normalized_segment& src) noexcept
    -> curves_packed_segment {
  auto dst = curves_packed_segment{};

  // Coefficients.
  for (int i = 0; i < 4; ++i) {
    dst.v[i] = src.poly.coeffs[i] << CURVES_SEGMENT_COEFFICIENT_SHIFT;
  }

  // Scatter inverse width.
  const auto inv_width = src.inv_width.value & CURVES_SEGMENT_MASK;
  dst.v[0] |= inv_width & CURVES_SEGMENT_PAYLOAD_MASK;
  dst.v[1] |=
      (inv_width >> CURVES_SEGMENT_PAYLOAD_BITS) & CURVES_SEGMENT_PAYLOAD_MASK;
  dst.v[2] |= (inv_width >> 2 * CURVES_SEGMENT_PAYLOAD_BITS)
              << (CURVES_SEGMENT_PAYLOAD_FIELD_BITS * 2);

  // Shifts.
  dst.v[2] |= (src.poly.relative_shifts[0] & CURVES_SEGMENT_PAYLOAD_FIELD_MASK);
  dst.v[2] |= (src.inv_width.shift & CURVES_SEGMENT_PAYLOAD_FIELD_MASK)
              << CURVES_SEGMENT_PAYLOAD_FIELD_BITS;
  dst.v[3] |= (src.poly.relative_shifts[1] & CURVES_SEGMENT_PAYLOAD_FIELD_MASK);
  dst.v[3] |= (src.poly.relative_shifts[2] & CURVES_SEGMENT_PAYLOAD_FIELD_MASK)
              << CURVES_SEGMENT_PAYLOAD_FIELD_BITS;
  dst.v[3] |= (src.poly.relative_shifts[3] & CURVES_SEGMENT_PAYLOAD_TOP_MASK)
              << (CURVES_SEGMENT_PAYLOAD_FIELD_BITS * 2);

  return dst;
}

}  // namespace curves
