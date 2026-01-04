// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "construction.hpp"
#include <curves/testing/test.hpp>
#include <curves/math/segment/packing.hpp>
#include <curves/math/segment/view.hpp>
#include <ranges>

namespace curves::segment {
namespace {

// Tests that the struct decodes correctly when flooded with all bits set.
TEST(SegmentConstruction, BitSaturationAndCollision) {
  // Fill the packed struct with pure 1s.
  // This sets every coefficient, exponent, shift, and payload bit to 1.
  curves_packed_segment packed;
  std::ranges::fill(packed.v, 0xFFFFFFFFFFFFFFFFULL);

  // Unpack.
  const auto unpacked = unpack(packed);

  // Verify shifts.
  //
  // All payload bits are 1, so poly shifts should be 62 since these are
  // denormals. inv_width does not use denormals, so its shift should be 63.
  EXPECT_EQ(unpacked.poly.shifts[0], 62);
  EXPECT_EQ(unpacked.poly.shifts[1], 62);
  EXPECT_EQ(unpacked.poly.shifts[2], 62);
  EXPECT_EQ(unpacked.poly.shifts[3], 62);
  EXPECT_EQ(unpacked.inv_width.shift, 63);

  // Verify signed coeffs.
  //
  // Storage: 45 bits of 1s (0x1FFFFFFFFFFF)
  // Mantissa (0-43): 44 bits of 1s (0xFFFFFFFFFFF)
  // Exponent: 63 -> denormal
  // Implicit bit: not restored because denormal
  // Sign bit (44): 1 (negative)
  // Result: -mantissa = -(2^44 - 1) = -0xFFFFFFFFFFF
  const auto expected_signed_coeff = -0xFFFFFFFFFFFLL;
  EXPECT_EQ(unpacked.poly.coeffs[0], expected_signed_coeff);
  EXPECT_EQ(unpacked.poly.coeffs[1], expected_signed_coeff);

  // Verify unsigned coeffs.
  //
  // Storage: 45 bits of 1s (0x1FFFFFFFFFFF)
  // Mantissa (0-44): 45 bits of 1s (0xFFFFFFFFFFF)
  // Exponent: 63 -> denormal mode
  // Implicit bit: not restored because denormal
  // Result: mantissa = 2^45 - 1
  const auto expected_unsigned_coeff = 0x1FFFFFFFFFFFLL;
  EXPECT_EQ(unpacked.poly.coeffs[2], expected_unsigned_coeff);
  EXPECT_EQ(unpacked.poly.coeffs[3], expected_unsigned_coeff);

  // Verify inv_width.
  //
  // Storage: 46 bits of 1s, reconstructed from scattered bits
  // Mantissa (0-45): 46 bits of all 1s (0x3FFFFFFFFFFF)
  // Exponent: 63 -> literally 2^63 because inv_width has no denormal mode
  // Implicit bit: always restored
  // Result: mantissa = (2^46 - 1) | (1 << 46) = 47 bits of 1s = 0x7FFFFFFFFFFF
  const auto expected_inv_width = 0x7FFFFFFFFFFFULL;
  EXPECT_EQ(unpacked.inv_width.value, expected_inv_width);
}

}  // namespace
}  // namespace curves::segment
