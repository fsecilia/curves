// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <curves/testing/test.hpp>
#include <curves/math/segment_packing.hpp>
#include <random>

namespace curves {
namespace {

// ----------------------------------------------------------------------------
// Packing
// ----------------------------------------------------------------------------

struct SegmentPackingTest : Test {
  std::mt19937_64 rng{0xF12345678};

  auto random_shift() noexcept -> int8_t {
    return static_cast<int8_t>(rng() & 63) - 32;
  }

  static auto sign_extend(int64_t value, int bits) -> int64_t {
    const auto shift = 64 - bits;
    return (static_cast<int64_t>(value) << shift) >> shift;
  }

  auto random_segment() noexcept -> curves_normalized_segment {
    curves_normalized_segment segment;

    for (auto& coeff : segment.coeffs) {
      coeff =
          sign_extend(rng() & CURVES_SEGMENT_MASK, CURVES_SEGMENT_FRAC_BITS);
    }

    segment.inv_width = rng() & CURVES_SEGMENT_MASK;

    // Write random values for internal shifts.
    for (auto shift = 0; shift < CURVES_SEGMENT_COEFF_COUNT - 1; ++shift) {
      segment.relative_shifts[shift] = random_shift();
    }

    // Final shift has extra bit.
    segment.relative_shifts[CURVES_SEGMENT_COEFF_COUNT - 1] =
        static_cast<int8_t>(rng() & 127) - 64;

    // inv_width_shift is unsigned.
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

TEST_F(SegmentPackingTest, RelativeShiftsMasked) {
  auto segment = curves_normalized_segment{};

  /*
    Set the first bit outside of the range, taking care not to set the sign
    bit, and make sure the bit is cleared during packing.
  */
  const auto expected = int8_t{10};
  const auto garbage_bit = int8_t{64};

  segment.relative_shifts[0] = expected | garbage_bit;
  segment.relative_shifts[1] = expected | garbage_bit;
  segment.relative_shifts[2] = expected | garbage_bit;

  // The last field is 7 bits. Garbage bit must be 128 (bit 7).
  segment.relative_shifts[3] = static_cast<int8_t>(expected | 128);

  const auto packed = curves_pack_segment(segment);
  const auto unpacked = curves_unpack_segment(&packed);

  EXPECT_EQ(unpacked.relative_shifts[0], expected);
  EXPECT_EQ(unpacked.relative_shifts[1], expected);
  EXPECT_EQ(unpacked.relative_shifts[2], expected);
  EXPECT_EQ(unpacked.relative_shifts[3], expected);

  // Check garbage didn't spill into neighbors
  EXPECT_EQ(unpacked.coeffs[3], 0);
  EXPECT_EQ(unpacked.inv_width_shift, 0);
}

TEST_F(SegmentPackingTest, InvWidthShiftMasked) {
  auto segment = curves_normalized_segment{};

  const auto expected = int8_t{10};
  const auto garbage_bit = int8_t{64};

  segment.inv_width_shift = expected | garbage_bit;

  const auto packed = curves_pack_segment(segment);
  const auto unpacked = curves_unpack_segment(&packed);

  EXPECT_EQ(unpacked.inv_width_shift, expected);

  // Check garbage didn't spill into neighbor.
  EXPECT_EQ(unpacked.inv_width, 0);
}

TEST_F(SegmentPackingTest, InvWidthMasked) {
  auto segment = curves_normalized_segment{};

  const auto expected = uint64_t{10};
  const auto garbage_bit = uint64_t{1} << CURVES_SEGMENT_FRAC_BITS;

  segment.inv_width = expected | garbage_bit;

  const auto packed = curves_pack_segment(segment);
  const auto unpacked = curves_unpack_segment(&packed);

  EXPECT_EQ(unpacked.inv_width, expected);

  // Check garbage didn't spill into neighbor.
  EXPECT_EQ(unpacked.coeffs[2], 0);
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
