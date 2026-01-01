// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <curves/testing/test.hpp>
#include <curves/math/segment.hpp>
#include <random>

namespace curves {
namespace {

// ----------------------------------------------------------------------------
// Segment Packing
// ----------------------------------------------------------------------------

struct SegmentPackingTest : ::testing::Test {
  std::mt19937_64 rng{0xF12345678};

  auto random_segment() noexcept -> curves_normalized_segment {
    curves_normalized_segment segment;

    for (auto& coeff : segment.coeffs) {
      coeff = static_cast<int64_t>(rng()) & ~CURVES_PAYLOAD_MASK;
    }

    // Mask inv width. It gets one more bit than the coeffs.
    static const auto inv_width_mask =
        CURVES_PAYLOAD_BITS | (CURVES_PAYLOAD_BITS >> 1);
    segment.inv_width = rng() & inv_width_mask;

    for (auto& shift : segment.relative_shifts) {
      shift = static_cast<int8_t>(rng() & 63) - 32;
    }

    segment.inv_width_shift = rng() & 63;

    return segment;
  }

  auto expect_segments_eq(const curves_normalized_segment& a,
                          const curves_normalized_segment& b) const noexcept {
    for (auto i = 0; i < 4; ++i) {
      EXPECT_EQ(a.coeffs[i], b.coeffs[i]) << "Coeff " << i << " mismatch";
    }
    EXPECT_EQ(a.inv_width, b.inv_width);
    for (auto i = 0; i < 4; ++i) {
      EXPECT_EQ(a.relative_shifts[i], b.relative_shifts[i])
          << "Shift " << i << " mismatch";
    }
    EXPECT_EQ(a.inv_width_shift, b.inv_width_shift);
  }
};

TEST_F(SegmentPackingTest, RoundTripFuzz) {
  for (auto i = 0; i < 10000; ++i) {
    const auto original = random_segment();
    const auto packed = curves_pack_segment(original);
    const auto unpacked = curves_unpack_segment(&packed);
    expect_segments_eq(original, unpacked);
  }
}

TEST_F(SegmentPackingTest, NegativeShiftsPreserved) {
  auto segment = curves_normalized_segment{};
  segment.relative_shifts[0] = -1;   // All 1s
  segment.relative_shifts[1] = -32;  // Min value

  const auto packed = curves_pack_segment(segment);
  const auto unpacked = curves_unpack_segment(&packed);

  EXPECT_EQ(unpacked.relative_shifts[0], -1);
  EXPECT_EQ(unpacked.relative_shifts[1], -32);
}

TEST_F(SegmentPackingTest, WalkingBit) {
  for (int bit = 0; bit < 256; ++bit) {
    auto packed = curves_packed_segment{};
    packed.v[bit >> 6] = (1ULL << (bit & 63));

    const auto unpacked = curves_unpack_segment(&packed);

    auto nonzero_fields = 0;

    // Count non-zero fields.
    for (auto c : unpacked.coeffs) {
      if (c) ++nonzero_fields;
    }

    if (unpacked.inv_width) ++nonzero_fields;

    for (auto s : unpacked.relative_shifts) {
      if (s) ++nonzero_fields;
    }
    if (unpacked.inv_width_shift) ++nonzero_fields;

    EXPECT_EQ(nonzero_fields, 1) << "Setting bit " << bit << " affected "
                                 << nonzero_fields << " fields.";

    // Check that repacking only sets the same bit.
    const auto repacked = curves_pack_segment(unpacked);
    EXPECT_THAT(packed.v, ElementsAreArray(repacked.v));
  }
}

}  // namespace
}  // namespace curves
