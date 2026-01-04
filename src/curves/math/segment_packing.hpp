// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Floating-point segment packing for kernel spline segments.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/segment_eval.hpp>

extern "C" {
#include <curves/driver/segment_unpacking.h>
}  // extern "C"

namespace curves::segment {

/*
 * Helper: Distributes storage-ready components into the packed struct layout.
 *
 * @coeff_storage: 4 coeffs, Sign-Magnitude, Implicit 1 stripped.
 * @iw_storage: InvWidth, Implicit 1 stripped.
 * @exponents: 4 exponents.
 * @iw_shift: InvWidth shift.
 */
inline curves_packed_segment __curves_pack_segment_layout(
    const u64* coeff_storage, const u8* exponents, u64 iw_storage,
    u8 iw_shift) {
  struct curves_packed_segment dst = {0};

  // 1. Pack coefficients (Top 45 bits)
  for (int i = 0; i < CURVES_SEGMENT_COEFF_COUNT; ++i) {
    dst.v[i] = coeff_storage[i] << CURVES_SEGMENT_COEFF_SHIFT;
  }

  // 2. Prepare Payload Components
  u64 ish = iw_shift & 0x3F;
  u64 e0 = exponents[0] & 0x3F;
  u64 e1 = exponents[1] & 0x3F;
  u64 e2 = exponents[2] & 0x3F;
  u64 e3 = exponents[3] & 0x3F;

  // 3. Pack Payloads (Bottom 19 bits)

  // v[0]: IW[0..18]
  dst.v[0] |= iw_storage & 0x7FFFF;

  // v[1]: IW[19..37]
  dst.v[1] |= (iw_storage >> 19) & 0x7FFFF;

  // v[2]: Exp0 (6) | InvShift (6) | IW[38..44] (7 bits)
  u64 v2_payload = e0 | (ish << 6) | (((iw_storage >> 38) & 0x7F) << 12);
  dst.v[2] |= v2_payload;

  // v[3]: Exp1 (6) | Exp2 (6) | Exp3 (6) | IW[45] (1)
  u64 v3_payload =
      e1 | (e2 << 6) | (e3 << 12) | (((iw_storage >> 45) & 1) << 18);
  dst.v[3] |= v3_payload;

  return dst;
}

/*
 * Helper: Converts 2's Comp s64 -> Sign-Mag + Storage Logic
 * - Handles Normalized (Shift 0-62, Implicit Strip)
 * - Handles Denormal   (Shift 63,   Explicit)
 */
/*
 * Helper: Converts 2's Comp s64 (Math Format) -> Sign-Mag (Storage Format).
 * - If Normal: Bit 44 is 1. We mask it off.
 * - If Denormal: Bit 44 is 0. We mask it off (no-op).
 */
static inline u64 __curves_coeff_math_to_storage(s64 val) {
  if (val == 0) return 0;

  // 1. Extract Sign
  // If val < 0, we want a 1 at bit 44 (the sign bit position in storage).
  u64 sign = (val < 0) ? (1ULL << CURVES_COEFF_SIGN_BIT_IDX) : 0ULL;

  // 2. Extract Magnitude
  u64 mag = (val < 0) ? -val : val;

  // 3. Strip Implicit Bit (Unconditional)
  // We keep only the bottom 44 bits.
  // - Normals lose their implicit 1 (good).
  // - Denormals (which are < 2^44) are untouched (good).
  u64 stored_mantissa = mag & ((1ULL << CURVES_COEFF_IMPLICIT_BIT_IDX) - 1);

  return sign | stored_mantissa;
}

/*
 * Helper: Converts Unsigned s64 (Math Format) -> Storage Format
 * - Implicit 1 is at bit 45.
 * - We store bits 0..44.
 */
static inline u64 __curves_coeff_math_to_storage_unsigned(s64 val) {
  if (val == 0) return 0;

  // This path requires positive values to put into unsigneds.
  assert(val > 0);

  // Strip Implicit 1 (Bit 45)
  // We keep the bottom 45 bits (0..44).
  // Denormals (val < 2^45) are untouched.
  return static_cast<u64>(val) & ((1ULL << 45) - 1);
}

/*
 * Packing: Packs normalized segment into packed, cache-friendly format.
 * * Layout (19 bit payloads):
 * v[0]: IW[0..18]
 * v[1]: IW[19..37]
 * v[2]: Exp0(6) | InvShift(6) | IW[38..44](7)
 * v[3]: Exp1(6) | Exp2(6) | Exp3(6) | IW[45](1)
 */
inline curves_packed_segment curves_pack_segment(
    const struct curves_normalized_segment* src) {
  u64 coeff_storage[CURVES_SEGMENT_COEFF_COUNT];

  // Cubic coefficients a and b are signed.
  coeff_storage[0] = __curves_coeff_math_to_storage(src->poly.coeffs[0]);
  coeff_storage[1] = __curves_coeff_math_to_storage(src->poly.coeffs[1]);

  // Coefficients c and d are unsigned because the functions we're
  // approximating are monotonically increasing by design.
  coeff_storage[2] =
      __curves_coeff_math_to_storage_unsigned(src->poly.coeffs[2]);
  coeff_storage[3] =
      __curves_coeff_math_to_storage_unsigned(src->poly.coeffs[3]);

  // 1. Convert Coeffs to Storage Format
  for (int i = 0; i < CURVES_SEGMENT_COEFF_COUNT; ++i) {
    coeff_storage[i] = __curves_coeff_math_to_storage(src->poly.coeffs[i]);
  }

  // 2. Strip Implicit 1 from Inv Width
  u64 iw_storage = src->inv_width.value & CURVES_INV_WIDTH_STORAGE_MASK;

  // 3. Delegate to Layout Helper
  return __curves_pack_segment_layout(coeff_storage, src->poly.exponents,
                                      iw_storage, src->inv_width.shift);
}

}  // namespace curves::segment
