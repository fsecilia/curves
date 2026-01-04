// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2026 Frank Secilia
*/

#include <curves/testing/test.hpp>
#include <curves/math/segment/construction.hpp>
#include <curves/math/segment/packing.hpp>
#include <curves/math/segment/segment.hpp>
#include <random>
#include <ranges>

namespace curves::segment {
namespace {

// ----------------------------------------------------------------------------
// Packing Round-Trip Tests
// ----------------------------------------------------------------------------

struct SegmentPackingTest : Test {
  std::mt19937_64 rng{0xF12345678};
  std::uniform_real_distribution<real_t> mantissa_dist{-1.0, 1.0};
  std::uniform_int_distribution<int> exp_dist{-70, 50};
  std::uniform_int_distribution<int> width_exp_dist{-20, 20};

  /*!
    Generates a random normalized segment valid for round-tripping.
  */
  auto random_segment(int i) noexcept -> NormalizedSegment {
    SegmentParams params;

    // Randomize signed coeffs.
    for (auto j = 0; j < 2; ++j) {
      if (i + j % 100 == 0) {
        params.coeffs[j] = 0.0;
      } else {
        params.coeffs[j] = std::ldexp(mantissa_dist(rng), exp_dist(rng));
      }
    }

    // Randomize unsigned coeffs.
    for (auto j = 2; j < kCoeffCount; ++j) {
      if (i + j % 100 == 0) {
        params.coeffs[j] = 0.0;
      } else {
        params.coeffs[j] =
            std::ldexp(std::abs(mantissa_dist(rng)), exp_dist(rng));
      }
    }

    // Randomize nonzero width.
    params.width =
        std::ldexp(std::fabs(mantissa_dist(rng)) + 1e-9, width_exp_dist(rng));

    return create_segment(params);
  }
};

TEST_F(SegmentPackingTest, RoundTripFuzz) {
  for (auto i = 0; i < 10000; ++i) {
    const auto original = random_segment(i);
    const auto packed = pack(original);
    const auto unpacked = unpack(packed);
    EXPECT_EQ(original, unpacked);
  }
}

TEST_F(SegmentPackingTest, ZeroSegmentRoundTrips) {
  NormalizedSegment segment = {};

  const auto packed = pack(segment);
  const auto unpacked = unpack(packed);

  for (auto i = 0; i < kCoeffCount; ++i) {
    EXPECT_EQ(unpacked.poly.coeffs[i], 0) << "Coeff " << i << " should be 0";
  }
}

TEST_F(SegmentPackingTest, ShiftsMaskedTo6Bits) {
  auto params = SegmentParams{};
  std::ranges::fill(params.coeffs, 1.0);
  params.width = 1.0;

  auto segment = create_segment(params);

  // Set shifts with garbage in upper bits.
  const auto expected = uint8_t{10};
  const auto garbage = uint8_t{0x80};
  for (auto i = 0; i < kCoeffCount; ++i) {
    segment.poly.shifts[i] = expected | garbage;
  }
  segment.inv_width.shift = expected | garbage;

  // Round trip.
  const auto packed = pack(segment);
  const auto unpacked = unpack(packed);

  // Verify garbage was stripped.
  for (auto i = 0; i < kCoeffCount; ++i) {
    EXPECT_EQ(unpacked.poly.shifts[i], expected)
        << "Poly shift " << i << " was not masked to 6 bits";
  }
  EXPECT_EQ(unpacked.inv_width.shift, expected)
      << "Inv_width shift was not masked to 6 bits";
}

TEST_F(SegmentPackingTest, InvWidthShiftMaskedTo6Bits) {
  auto segment = NormalizedSegment{};

  const uint8_t expected = 42;
  const uint8_t garbage = 0x80;

  segment.inv_width.shift = expected | garbage;

  const auto packed = pack(segment);
  const auto unpacked = unpack(packed);

  EXPECT_EQ(unpacked.inv_width.shift, expected);
}

TEST_F(SegmentPackingTest, SignedCoeffsPreserveSign) {
  auto segment = NormalizedSegment{};

  // Positive value with implicit 1 at bit 44.
  segment.poly.coeffs[0] = (1LL << kSignedImplicitBit) | 0x123456789AB;
  segment.poly.shifts[0] = 30;

  // Negative value.
  segment.poly.coeffs[1] = -((1LL << kSignedImplicitBit) | 0xABCDEF01234);
  segment.poly.shifts[1] = 25;

  const auto packed = pack(segment);
  const auto unpacked = unpack(packed);

  EXPECT_EQ(unpacked.poly.coeffs[0], segment.poly.coeffs[0]);
  EXPECT_GT(unpacked.poly.coeffs[0], 0) << "Coeff 0 should be positive";

  EXPECT_EQ(unpacked.poly.coeffs[1], segment.poly.coeffs[1]);
  EXPECT_LT(unpacked.poly.coeffs[1], 0) << "Coeff 1 should be negative";
}

TEST_F(SegmentPackingTest, UnsignedCoeffsAlwaysPositive) {
  auto segment = NormalizedSegment{};

  // Set c and d with implicit 1 at bit 45.
  segment.poly.coeffs[2] = (1LL << kUnsignedImplicitBit) | 0x1FFFFFFFF;
  segment.poly.shifts[2] = 20;

  segment.poly.coeffs[3] = (1LL << kUnsignedImplicitBit) | 0x100000000;
  segment.poly.shifts[3] = 15;

  const auto packed = pack(segment);
  const auto unpacked = unpack(packed);

  EXPECT_EQ(unpacked.poly.coeffs[2], segment.poly.coeffs[2]);
  EXPECT_GT(unpacked.poly.coeffs[2], 0) << "Coeff 2 (c) should be positive";

  EXPECT_EQ(unpacked.poly.coeffs[3], segment.poly.coeffs[3]);
  EXPECT_GT(unpacked.poly.coeffs[3], 0) << "Coeff 3 (d) should be positive";
}

}  // namespace
}  // namespace curves::segment
