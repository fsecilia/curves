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

namespace curves::segment {

// Masks coefficients and inv_width.
constexpr auto CURVES_SEGMENT_MASK = (1ULL << CURVES_SEGMENT_FRAC_BITS) - 1;

// Masks top payload field.
constexpr auto CURVES_SEGMENT_PAYLOAD_TOP_MASK =
    (1ULL << CURVES_SEGMENT_PAYLOAD_TOP_BITS) - 1;

auto pack_segment(const curves_normalized_segment& src) noexcept
    -> curves_packed_segment;

}  // namespace curves::segment
