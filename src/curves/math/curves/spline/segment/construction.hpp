// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Floating-point to normalized segment construction.

  Handles conversion from floating-point coefficients and width to the
  normalized segment format. The flow is:

    float -> storage (packed wire format) -> normalized (math format)

  By going through the packed format, we ensure the resulting normalized
  segment is bit-identical to what the kernel produces when unpacking.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/curves/cubic.hpp>
#include <curves/math/curves/spline/segment/packing.hpp>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>

namespace curves::segment {

// ----------------------------------------------------------------------------
// Intermediate Result Type
// ----------------------------------------------------------------------------

/// Result of normalizing a floating-point value for storage.
struct StorageValue {
  uint64_t mantissa;  ///< Mantissa with implicit 1 stripped.
  uint8_t shift;      ///< Right-shift to recover original scale.
};

// ----------------------------------------------------------------------------
// Float -> Storage Converters
// ----------------------------------------------------------------------------

/*!
  Normalizes a signed coefficient to storage format.

  Places the MSB at the implicit bit position, strips the implicit 1, and
  packs the sign bit. For very small values that can't be normalized with
  shift <= 62, uses kDenormalShift (63) as a sentinel to indicate the
  implicit bit is not present.

  @param val Signed floating-point coefficient.
  @return Storage format: sign at bit 44, mantissa in [0..43].
*/
inline auto pack_signed_coeff(real_t val) noexcept -> StorageValue {
  if (val == 0.0L) {
    return {0, kDenormalShift};
  }

  // Extract sign and magnitude.
  const auto sign = (val < 0) ? 1ULL : 0ULL;
  const auto mag = std::abs(val);

  // Get exponent: mag = mantissa * 2^exp where mantissa in [0.5, 1.0).
  int exp;
  std::frexp(mag, &exp);

  // Calculate shift to place MSB at implicit bit position.
  const auto ideal_shift = kSignedImplicitBit - (exp - 1);
  int actual_shift;
  uint8_t stored_shift;
  const auto is_denormal = ideal_shift >= kDenormalShift;
  if (is_denormal) {
    actual_shift = kDenormalShift - 1;
    stored_shift = kDenormalShift;
  } else {
    actual_shift = std::max(0, ideal_shift);
    stored_shift = static_cast<uint8_t>(actual_shift);
  }

  // Scale and convert to integer.
  const auto scaled = std::ldexp(mag, actual_shift);
  const auto norm = static_cast<uint64_t>(std::round(scaled));

  // Strip implicit 1 and pack sign.
  const auto mantissa = norm & kSignedMantissaMask;
  const auto storage = (sign << kSignBit) | mantissa;

  return {storage, stored_shift};
}

/*!
  Normalizes an unsigned coefficient to storage format.

  Unsigned coefficients (c, d) have implicit 1 at bit 45, giving 46 bits of
  effective precision. Negative values are clamped to zero.

  @param val Unsigned floating-point coefficient (should be non-negative).
  @return Storage format: 45-bit mantissa with implicit 1 stripped.
*/
inline auto pack_unsigned_coeff(real_t val) noexcept -> StorageValue {
  // Clamp negative to zero (shouldn't occur for monotonic curves).
  if (val <= 0.0L) {
    return {0, kDenormalShift};
  }

  int exp;
  std::frexp(val, &exp);

  // Implicit 1 at bit 45 for unsigned coefficients.
  const auto ideal_shift = kUnsignedImplicitBit - (exp - 1);
  int actual_shift;
  uint8_t stored_shift;

  if (ideal_shift > 62) {
    actual_shift = 62;
    stored_shift = kDenormalShift;
  } else {
    actual_shift = std::max(0, ideal_shift);
    stored_shift = static_cast<uint8_t>(actual_shift);
  }

  const auto scaled = std::ldexp(val, actual_shift);
  const auto norm = static_cast<uint64_t>(std::round(scaled));

  // Strip implicit 1 at bit 45.
  const auto mantissa = norm & kUnsignedMantissaMask;

  return {mantissa, stored_shift};
}

/*!
  Normalizes inverse width to storage format.

  Inverse width has implicit 1 at bit 46. Unlike coefficients, inverse width
  doesn't use a denormal representation - very wide segments that would
  require denormal are a logic error.

  @param val Inverse of segment width (must be positive).
  @return Storage format: 46-bit mantissa with implicit 1 stripped.
*/
inline auto pack_inv_width(real_t val) noexcept -> StorageValue {
  if (val <= 0.0L) {
    return {0, 0};
  }

  int exp;
  std::frexp(val, &exp);

  const auto ideal_shift = kInvWidthImplicitBit - (exp - 1);
  const auto actual_shift = std::clamp(ideal_shift, 0, 63);

  // Assert if segment is too wide for normalized representation.
  // assert(ideal_shift <= 63 && "Segment width exceeds maximum");

  const auto scaled = std::ldexp(val, actual_shift);
  const auto norm = static_cast<uint64_t>(std::round(scaled));

  // Strip implicit 1 at bit 46.
  const auto mantissa = norm & kInvWidthStorageMask;

  return {mantissa, static_cast<uint8_t>(actual_shift)};
}

// ----------------------------------------------------------------------------
// Segment Construction
// ----------------------------------------------------------------------------

/// Parameters for constructing a segment from floating-point values.
struct SegmentParams {
  cubic::Monomial<real_t> poly;  ///< Polynomial coefficients: a, b, c, d
  real_t width;                  ///< Segment width in x-space

  friend auto operator<<(std::ostream& out, const SegmentParams& src)
      -> std::ostream& {
    return out << "SegmentParams{.poly = " << src.poly
               << ", .width = " << src.width << "}";
  }
};

/*!
  Constructs a normalized segment from floating-point parameters.

  The construction goes through the packed (wire) format to ensure the
  resulting normalized segment is bit-identical to what the kernel produces.
  This guarantees that floating-point evaluation in the frontend matches
  fixed-point evaluation in the kernel.

  @param params Floating-point coefficients and width.
  @return Normalized segment ready for evaluation.
*/
inline auto create_segment(const SegmentParams& params) noexcept
    -> NormalizedSegment {
  uint64_t coeff_storage[kCoeffCount];
  uint8_t shifts[kCoeffCount];

  // Pack signed coefficients (a, b).
  for (int i = 0; i < 2; ++i) {
    const auto [mantissa, shift] = pack_signed_coeff(params.poly.coeffs[i]);
    coeff_storage[i] = mantissa;
    shifts[i] = shift;
  }

  // Pack unsigned coefficients (c, d).
  for (int i = 2; i < kCoeffCount; ++i) {
    const auto [mantissa, shift] = pack_unsigned_coeff(params.poly.coeffs[i]);
    coeff_storage[i] = mantissa;
    shifts[i] = shift;
  }

  // Pack inverse width.
  auto inv_width_packed = StorageValue{0, 0};
  if (params.width > 0.0L) {
    inv_width_packed = pack_inv_width(1.0L / params.width);
  }

  // Pack into wire format, then unpack to math format.
  // This ensures bit-identical representation to kernel unpacking.
  const auto packed = pack_layout(
      coeff_storage, shifts, inv_width_packed.mantissa, inv_width_packed.shift);

  return unpack(packed);
}

}  // namespace curves::segment
