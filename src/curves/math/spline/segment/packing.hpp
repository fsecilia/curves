// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Segment packing: normalized math format → packed wire format.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/math/spline/segment/segment.hpp>
#include <cassert>
#include <cstdint>

namespace curves::segment {

// ----------------------------------------------------------------------------
// Storage Format Converters (Math Format → Storage Format)
// ----------------------------------------------------------------------------

/*!
  Converts signed coefficient from 2's complement to sign-magnitude storage.

  Signed coefficients (a, b) have implicit 1 at bit 44 and sign at bit 44.
  Storage format strips the implicit 1 and places the sign bit there instead.
*/
inline auto coeff_to_storage_signed(int64_t val) noexcept -> uint64_t {
  if (val == 0) return 0;

  // Extract sign.
  const auto sign = (val < 0) ? (1ULL << kSignBit) : 0ULL;

  // Get magnitude and strip implicit 1.
  const auto mag =
      (val < 0) ? static_cast<uint64_t>(-val) : static_cast<uint64_t>(val);
  const auto mantissa = mag & kSignedMantissaMask;

  return sign | mantissa;
}

/*!
  Converts unsigned coefficient to storage format.

  Unsigned coefficients (c, d) have implicit 1 at bit 45.
  Storage format strips the implicit 1.
*/
inline auto coeff_to_storage_unsigned(int64_t val) noexcept -> uint64_t {
  if (val == 0) return 0;

  assert(val > 0 && "Unsigned coefficient must be positive");

  // Strip implicit 1 at bit 45.
  return static_cast<uint64_t>(val) & kUnsignedMantissaMask;
}

// ----------------------------------------------------------------------------
// Layout Packing
// ----------------------------------------------------------------------------

/*!
  Packs storage-format components into the wire format.

  @param coeff_storage  4 coefficients with implicit 1 stripped.
  @param shifts         4 coefficient shifts (6-bit each).
  @param inv_width      Inverse width with implicit 1 stripped.
  @param inv_width_shift Inverse width shift (6-bit).
  @return Packed segment ready for wire transmission.

  Layout (64 bits per word, 256 bits total):
    v[0]: coeff[0] (45) | inv_width[0..18] (19)
    v[1]: coeff[1] (45) | inv_width[19..37] (19)
    v[2]: coeff[2] (45) | iw[38..44] (7) | iw_shift (6) | shift[0] (6)
    v[3]: coeff[3] (45) | iw[45] (1) | shift[3] (6) | shift[2] (6) | shift[1]
  (6)
*/
inline auto pack_layout(const uint64_t* coeff_storage, const uint8_t* shifts,
                        uint64_t inv_width, uint8_t inv_width_shift) noexcept
    -> PackedSegment {
  auto dst = PackedSegment{};

  // Pack coefficients into top 45 bits.
  for (int i = 0; i < kCoeffCount; ++i) {
    dst.v[i] = coeff_storage[i] << kCoeffShift;
  }

  // Mask all shift values to 6 bits.
  const auto iw_sh = inv_width_shift & kShiftMask;
  const auto s0 = shifts[0] & kShiftMask;
  const auto s1 = shifts[1] & kShiftMask;
  const auto s2 = shifts[2] & kShiftMask;
  const auto s3 = shifts[3] & kShiftMask;

  // v[0]: inv_width bits [0..18]
  dst.v[0] |= inv_width & kPayloadMask;

  // v[1]: inv_width bits [19..37]
  dst.v[1] |= (inv_width >> 19) & kPayloadMask;

  // v[2]: shift[0] (6) | iw_shift (6) | inv_width[38..44] (7)
  dst.v[2] |= s0 | (iw_sh << 6) | (((inv_width >> 38) & 0x7F) << 12);

  // v[3]: shift[1] (6) | shift[2] (6) | shift[3] (6) | inv_width[45] (1)
  dst.v[3] |= s1 | (s2 << 6) | (s3 << 12) | (((inv_width >> 45) & 0x1) << 18);

  return dst;
}

// ----------------------------------------------------------------------------
// Segment Packing
// ----------------------------------------------------------------------------

/*!
  Packs a normalized segment into wire format.

  Converts coefficients from 2's complement to storage format, strips implicit
  leading 1 bits, and distributes bits across the packed structure.
*/
inline auto pack(const NormalizedSegment& src) noexcept -> PackedSegment {
  uint64_t coeff_storage[kCoeffCount];
  uint8_t shift_storage[kCoeffCount];

  // Signed coeffs, (a, b)
  for (int i = 0; i < 2; ++i) {
    const auto val = src.poly.coeffs[i];
    coeff_storage[i] = coeff_to_storage_signed(val);

    // Check if this is a Denormal/Zero in Math Format
    // (Shift is 62, but Implicit Bit 44 is NOT set in the value)
    const auto is_implicit_set =
        (static_cast<uint64_t>(std::abs(val)) & (1ULL << kSignedImplicitBit));
    if (!is_implicit_set) {
      shift_storage[i] = kDenormalShift;
    } else {
      shift_storage[i] = src.poly.shifts[i];
    }
  }

  // Unsigned coeffs, (c, d)
  for (int i = 2; i < kCoeffCount; ++i) {
    const auto val = src.poly.coeffs[i];
    coeff_storage[i] = coeff_to_storage_unsigned(val);

    // Check Implicit Bit 45
    const auto is_implicit_set =
        (static_cast<uint64_t>(val) & (1ULL << kUnsignedImplicitBit));
    if (!is_implicit_set) {
      shift_storage[i] = kDenormalShift;
    } else {
      shift_storage[i] = src.poly.shifts[i];
    }
  }

  // Strip implicit 1 from inverse width.
  const auto inv_width_storage = src.inv_width.value & kInvWidthStorageMask;

  // Pass 'wire_shifts' instead of 'src.poly.shifts'
  return pack_layout(coeff_storage, shift_storage, inv_width_storage,
                     src.inv_width.shift);
}

}  // namespace curves::segment
