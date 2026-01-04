// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Floating-point segment packing for kernel spline segments.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/segment_eval.hpp>
#include <curves/math/segment_packing.hpp>

namespace curves::segment {

/*!
  Packs unsigned Inv Width with Implicit 1.
  Normalizes 'val' such that the MSB is at 'target_bit'.
  Returns the masked payload (leading 1 stripped).
*/
static inline u64 pack_unsigned_implicit(double val, int target_bit,
                                         u8* out_exp) {
  if (val == 0.0) {
    *out_exp = 0;
    return 0;
  }

  int exp;
  std::frexp(val, &exp);

  // We want: val = (1.mantissa * 2^target_bit) * 2^-shift
  // shift = target_bit - (log2(val))
  int ideal_shift = target_bit - (exp - 1);

  // Clamp the shift to the available storage range [0, 63].
  int actual_shift = std::clamp(ideal_shift, 0, 63);
  *out_exp = static_cast<u8>(actual_shift);

  // Re-calculate the integer based on the final clamped shift.
  // norm = val * 2^shift
  double scaled = std::ldexp(val, actual_shift);

  // Convert to integer.
  u64 norm = static_cast<u64>(scaled);

  // Strip Implicit 1 (Bit target_bit)
  return norm & ((1ULL << target_bit) - 1);
}

/*
 * Helper: Packs signed coefficient with Signed Magnitude + Implicit 1.
 */
static inline u64 pack_signed_coeff_implicit(double val, int target_bit,
                                             u8* out_exp) {
  if (val == 0.0) {
    *out_exp = 0;
    return 0;
  }

  // 1. Sign
  u64 sign = (val < 0) ? 1ULL : 0ULL;
  double mag = std::abs(val);

  // 2. Normalize Magnitude
  int exp;
  std::frexp(mag, &exp);

  // Calculate Shift
  int ideal_shift = target_bit - (exp - 1);
  int actual_shift = std::clamp(ideal_shift, 0, 62);
  if (ideal_shift > 62) {
    *out_exp = 63;
  } else {
    *out_exp = static_cast<u8>(actual_shift);
  }

  // 3. Re-calculate Integer from Shift
  double scaled = std::ldexp(mag, actual_shift);
  u64 norm_mag = static_cast<u64>(scaled);

  // 4. Strip Implicit 1
  u64 stored_mantissa = norm_mag & ((1ULL << target_bit) - 1);

  // 5. Pack Sign (at target_bit) and Mantissa
  return (sign << target_bit) | stored_mantissa;
}

struct SegmentConstructionParams {
  double coeffs[4];
  double width;
};

/*
 * Construction: Converts raw doubles to Normalized segment via Packed
 * intermediate.
 */
inline auto curves_create_segment(const SegmentConstructionParams& params)
    -> curves_normalized_segment {
  u64 coeff_storage[CURVES_SEGMENT_COEFF_COUNT];
  u8 exponents[CURVES_SEGMENT_COEFF_COUNT];
  u64 iw_storage;
  u8 iw_shift;

  // 1. Convert Coeff Doubles -> Storage Bits (Sign-Mag + Stripped)
  for (int i = 0; i < 4; ++i) {
    coeff_storage[i] = pack_signed_coeff_implicit(
        params.coeffs[i], CURVES_COEFF_IMPLICIT_BIT_IDX, &exponents[i]);
  }

  // 2. Convert Width Double -> Storage Bits (Stripped)
  if (params.width > 0.0) {
    double inv = 1.0 / params.width;
    iw_storage = pack_unsigned_implicit(inv, CURVES_INV_WIDTH_IMPLICIT_BIT_IDX,
                                        &iw_shift);
  } else {
    iw_storage = 0;
    iw_shift = 0;
  }

  // 3. Create Packed Segment (The "Wire Format")
  struct curves_packed_segment packed = __curves_pack_segment_layout(
      coeff_storage, exponents, iw_storage, iw_shift);

  // 4. Unpack to get the "Math Format"
  // This guarantees the struct we use for eval is bit-identical to the
  // kernel's.
  return curves_unpack_segment(&packed);
}

}  // namespace curves::segment
