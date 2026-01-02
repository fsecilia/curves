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
// Evaluation
// ----------------------------------------------------------------------------

struct EvaluationTestVector {
  std::array<real_t, CURVES_CUBIC_COEFF_COUNT> coeffs;
  real_t width;
  real_t x0;
  real_t x;
  real_t expected_t;
  real_t expected_eval;
  real_t tolerance;

  friend auto operator<<(std::ostream& out, const EvaluationTestVector& src)
      -> std::ostream& {
    out << "{.coeffs = {" << src.coeffs[0];
    for (auto coeff = 1; coeff < CURVES_CUBIC_COEFF_COUNT; ++coeff) {
      out << ", " << src.coeffs[coeff];
    }
    out << "}, .width = " << src.width << ", .x0 = " << src.x0
        << ", .x = " << src.x << ", .expected_t = " << src.expected_t
        << ", .expected_eval = " << src.expected_eval
        << ", .tolerance = " << src.tolerance << "}";

    return out;
  }
};

auto ideal_shift(real_t value) -> int {
  static const auto mantissa_bits = 63;
  if (!value) return mantissa_bits;

  int exp;
  std::frexp(value, &exp);
  return mantissa_bits - exp;
}

static const auto poly_frac_bits = 32;

struct SegmentEvaluationTest : TestWithParam<EvaluationTestVector> {
  using Sut = SegmentView;

  static auto create_segment() noexcept -> curves_normalized_segment {
    curves_normalized_segment result;

    const auto float_coeffs = GetParam().coeffs;
    auto current_shift = ideal_shift(float_coeffs[0]);
    result.coeffs[0] = to_fixed(float_coeffs[0], current_shift);

    for (auto coeff = 1; coeff < CURVES_CUBIC_COEFF_COUNT; ++coeff) {
      const auto next_val = float_coeffs[coeff];
      const auto ideal_next = ideal_shift(next_val);
      const auto shift = current_shift - ideal_next;
      const auto clamped_shift = std::clamp(shift, -32, 31);
      result.relative_shifts[coeff - 1] = static_cast<s8>(clamped_shift);
      current_shift -= clamped_shift;
      result.coeffs[coeff] = to_fixed(next_val, current_shift);
    }

    const auto final_shift = current_shift - poly_frac_bits;
    result.relative_shifts[3] =
        static_cast<s8>(std::clamp(final_shift, -64, 63));

    const auto width = GetParam().width;
    if (width > 0.0) {
      const auto inv_width = 1.0 / width;
      result.inv_width_shift = ideal_shift(inv_width);
      result.inv_width = to_fixed(inv_width, result.inv_width_shift);
    } else {
      result.inv_width = 0;
      result.inv_width_shift = 0;
    }

    return result;
  }

  curves_normalized_segment segment = create_segment();
  Sut sut{&segment};
};

TEST_P(SegmentEvaluationTest, inv_width) {
  const auto actual = sut.inv_width();
  const auto expected = 1.0 / GetParam().width;
  EXPECT_NEAR(actual, expected, 1e-15);
}

TEST_P(SegmentEvaluationTest, x_to_t) {
  const auto actual = sut.x_to_t(GetParam().x, GetParam().x0);
  const auto expected = GetParam().expected_t;
  EXPECT_NEAR(actual, expected, 1e-12);
}

TEST_P(SegmentEvaluationTest, eval) {
  const auto t = sut.x_to_t(GetParam().x, GetParam().x0);

  const auto actual = to_real(sut.eval(t), poly_frac_bits);
  const auto expected = GetParam().expected_eval;
  EXPECT_NEAR(expected, actual, GetParam().tolerance);
}

const EvaluationTestVector evaluation_test_vectors[] = {
    {
        .coeffs = {9.5, -6.2, 3.1, 0.2},
        .width = 4.9,
        .x0 = 1.4,
        .x = 2.5,
        .expected_t = 0.224489795918,
        .expected_eval = 0.6909416994619L,
        .tolerance = 1.5e-10,
    },
};

INSTANTIATE_TEST_SUITE_P(evaluation_test_vectors, SegmentEvaluationTest,
                         ValuesIn(evaluation_test_vectors));

// ----------------------------------------------------------------------------
// Packing
// ----------------------------------------------------------------------------

struct SegmentPackingTest : Test {
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
