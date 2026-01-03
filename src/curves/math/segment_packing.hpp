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

extern const uint64_t CURVES_SEGMENT_MASK;
extern const u64 CURVES_SEGMENT_PAYLOAD_TOP_MASK;

auto pack_segment(const curves_normalized_segment& src) noexcept
    -> curves_packed_segment;

}  // namespace curves::segment
