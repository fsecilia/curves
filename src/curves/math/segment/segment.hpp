// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Facade for kernel segment definitions.

  This header wraps the kernel's C types and functions, providing clean C++
  interfaces with proper naming conventions. All C++ code should use these
  types and constants rather than accessing the kernel headers directly.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

extern "C" {
#include <curves/driver/segment/eval.h>
#include <curves/driver/segment/unpacking.h>
}

#include <cstdint>

namespace curves::segment {

// ----------------------------------------------------------------------------
// Type Aliases
// ----------------------------------------------------------------------------

using NormalizedSegment = curves_normalized_segment;
using NormalizedPoly = curves_normalized_poly;
using NormalizedInvWidth = curves_normalized_inv_width;
using PackedSegment = curves_packed_segment;

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

// Polynomial structure.
inline constexpr int kCoeffCount = CURVES_SEGMENT_COEFF_COUNT;

// Fixed-point precision.
inline constexpr auto kInputFracBits = 48;
inline constexpr int kOutputFracBits = CURVES_SEGMENT_OUT_FRAC_BITS;
inline constexpr int kTFracBits = CURVES_SEGMENT_T_FRAC_BITS;

// Packed storage layout.
inline constexpr int kCoeffStorageBits = CURVES_SEGMENT_COEFF_STORAGE_BITS;
inline constexpr int kCoeffShift = CURVES_SEGMENT_COEFF_SHIFT;
inline constexpr int kPayloadBits = kCoeffShift;
inline constexpr auto kPayloadMask = (1ULL << kPayloadBits) - 1;

// Signed coefficients (a, b): implicit 1 and sign at bit 44.
inline constexpr int kSignedImplicitBit = CURVES_COEFF_SIGNED_IMPLICIT_BIT;
inline constexpr int kSignBit = CURVES_COEFF_SIGN_BIT;
inline constexpr int kSignedMantissaBits = kSignedImplicitBit;
inline constexpr auto kSignedMantissaMask = (1ULL << kSignedMantissaBits) - 1;

// Unsigned coefficients (c, d): implicit 1 at bit 45.
inline constexpr int kUnsignedImplicitBit = CURVES_COEFF_UNSIGNED_IMPLICIT_BIT;
inline constexpr int kUnsignedMantissaBits = kUnsignedImplicitBit;
inline constexpr auto kUnsignedMantissaMask =
    (1ULL << kUnsignedMantissaBits) - 1;

// Inverse width: implicit 1 at bit 46.
inline constexpr int kInvWidthImplicitBit = CURVES_INV_WIDTH_IMPLICIT_BIT;
inline constexpr int kInvWidthStorageBits = CURVES_INV_WIDTH_STORAGE_BITS;
inline constexpr uint64_t kInvWidthStorageMask = CURVES_INV_WIDTH_STORAGE_MASK;

// Shift encoding.
inline constexpr int kShiftBits = CURVES_SHIFT_BITS;
inline constexpr uint64_t kShiftMask = CURVES_SHIFT_MASK;
inline constexpr int kDenormalShift = CURVES_DENORMAL_SHIFT;

// ----------------------------------------------------------------------------
// Function Wrappers
// ----------------------------------------------------------------------------

/// Unpacks segment from wire format to math format.
inline auto unpack(const PackedSegment& src) noexcept -> NormalizedSegment {
  return curves_unpack_segment(&src);
}

/// Evaluates polynomial at normalized t.
inline auto eval_poly(const NormalizedPoly& poly, uint64_t t) noexcept
    -> int64_t {
  return __curves_segment_eval_poly(&poly, t);
}

/// Converts spline x to segment-local t.
inline auto x_to_t(const NormalizedInvWidth& inv_width, int64_t x, int64_t x0,
                   unsigned int x_frac_bits) noexcept -> uint64_t {
  return __curves_segment_x_to_t(&inv_width, x, x0, x_frac_bits);
}

}  // namespace curves::segment

inline auto operator==(const curves::segment::NormalizedSegment& a,
                       const curves::segment::NormalizedSegment& b) noexcept {
  if (a.inv_width.value != b.inv_width.value) return false;
  if (a.inv_width.shift != b.inv_width.shift) return false;

  for (auto i = 0; i < 4; ++i) {
    // kDenormalShift should never be >= kDenormalShift in a normalized segment.
    assert(a.poly.shifts[i] != curves::segment::kDenormalShift);
    assert(b.poly.shifts[i] != curves::segment::kDenormalShift);

    if (a.poly.coeffs[i] != b.poly.coeffs[i]) return false;
    if (a.poly.shifts[i] != b.poly.shifts[i]) return false;
  }
  return true;
}
