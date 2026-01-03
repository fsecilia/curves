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

    for (auto& coeff : segment.poly.coeffs) {
      coeff =
          sign_extend(rng() & CURVES_SEGMENT_MASK, CURVES_SEGMENT_FRAC_BITS);
    }

    segment.inv_width.value = rng() & CURVES_SEGMENT_MASK;

    // Write random values for internal shifts.
    for (auto shift = 0; shift < CURVES_SEGMENT_COEFF_COUNT - 1; ++shift) {
      segment.poly.relative_shifts[shift] = random_shift();
    }

    // Final shift has extra bit.
    segment.poly.relative_shifts[CURVES_SEGMENT_COEFF_COUNT - 1] =
        static_cast<int8_t>(rng() & 127) - 64;

    // inv_width_shift is unsigned.
    segment.inv_width.shift = rng() & 63;

    return segment;
  }

  auto expect_segments_eq(const curves_normalized_segment& a,
                          const curves_normalized_segment& b) const noexcept {
    for (auto i = 0; i < 4; ++i) {
      EXPECT_EQ(a.poly.coeffs[i], b.poly.coeffs[i])
          << "Coeff " << i << " mismatch";
    }
    for (auto i = 0; i < 4; ++i) {
      EXPECT_EQ(a.poly.relative_shifts[i], b.poly.relative_shifts[i])
          << "Shift " << i << " mismatch";
    }
    EXPECT_EQ(a.inv_width.value, b.inv_width.value);
    EXPECT_EQ(a.inv_width.shift, b.inv_width.shift);
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
  segment.poly.relative_shifts[0] = -1;   // All 1s
  segment.poly.relative_shifts[1] = -32;  // Min value

  const auto packed = curves_pack_segment(segment);
  const auto unpacked = curves_unpack_segment(&packed);

  EXPECT_EQ(unpacked.poly.relative_shifts[0], -1);
  EXPECT_EQ(unpacked.poly.relative_shifts[1], -32);
}

TEST_F(SegmentPackingTest, RelativeShiftsMasked) {
  auto segment = curves_normalized_segment{};

  /*
    Set the first bit outside of the range, taking care not to set the sign
    bit, and make sure the bit is cleared during packing.
  */
  const auto expected = int8_t{10};
  const auto garbage_bit = int8_t{64};

  segment.poly.relative_shifts[0] = expected | garbage_bit;
  segment.poly.relative_shifts[1] = expected | garbage_bit;
  segment.poly.relative_shifts[2] = expected | garbage_bit;

  // The last field is 7 bits. Garbage bit must be 128 (bit 7).
  segment.poly.relative_shifts[3] = static_cast<int8_t>(expected | 128);

  const auto packed = curves_pack_segment(segment);
  const auto unpacked = curves_unpack_segment(&packed);

  EXPECT_EQ(unpacked.poly.relative_shifts[0], expected);
  EXPECT_EQ(unpacked.poly.relative_shifts[1], expected);
  EXPECT_EQ(unpacked.poly.relative_shifts[2], expected);
  EXPECT_EQ(unpacked.poly.relative_shifts[3], expected);

  // Check garbage didn't spill into neighbors
  EXPECT_EQ(unpacked.poly.coeffs[3], 0);
  EXPECT_EQ(unpacked.inv_width.shift, 0);
}

TEST_F(SegmentPackingTest, InvWidthShiftMasked) {
  auto segment = curves_normalized_segment{};

  const auto expected = int8_t{10};
  const auto garbage_bit = int8_t{64};

  segment.inv_width.shift = expected | garbage_bit;

  const auto packed = curves_pack_segment(segment);
  const auto unpacked = curves_unpack_segment(&packed);

  EXPECT_EQ(unpacked.inv_width.shift, expected);

  // Check garbage didn't spill into neighbor.
  EXPECT_EQ(unpacked.inv_width.value, 0);
}

TEST_F(SegmentPackingTest, InvWidthMasked) {
  auto segment = curves_normalized_segment{};

  const auto expected = uint64_t{10};
  const auto garbage_bit = uint64_t{1} << CURVES_SEGMENT_FRAC_BITS;

  segment.inv_width.value = expected | garbage_bit;

  const auto packed = curves_pack_segment(segment);
  const auto unpacked = curves_unpack_segment(&packed);

  EXPECT_EQ(unpacked.inv_width.value, expected);

  // Check garbage didn't spill into neighbor.
  EXPECT_EQ(unpacked.poly.coeffs[2], 0);
}

TEST_F(SegmentPackingTest, WalkingBit) {
  for (int bit = 0; bit < 256; ++bit) {
    auto packed = curves_packed_segment{};
    packed.v[bit >> 6] = (1ULL << (bit & 63));

    const auto unpacked = curves_unpack_segment(&packed);

    auto nonzero_fields = 0;

    // Count non-zero fields.
    for (auto c : unpacked.poly.coeffs) {
      if (c) ++nonzero_fields;
    }
    for (auto s : unpacked.poly.relative_shifts) {
      if (s) ++nonzero_fields;
    }
    if (unpacked.inv_width.value) ++nonzero_fields;
    if (unpacked.inv_width.shift) ++nonzero_fields;

    EXPECT_EQ(nonzero_fields, 1) << "Setting bit " << bit << " affected "
                                 << nonzero_fields << " fields.";

    // Check that repacking only sets the same bit.
    const auto repacked = curves_pack_segment(unpacked);
    EXPECT_THAT(packed.v, ElementsAreArray(repacked.v));
  }
}

}  // namespace
}  // namespace curves
