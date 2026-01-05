// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "view.hpp"
#include <curves/testing/test.hpp>
#include <curves/math/segment/construction.hpp>

namespace curves::segment {
namespace {

// ----------------------------------------------------------------------------
// Evaluation
// ----------------------------------------------------------------------------

struct EvaluationTestVector {
  std::array<real_t, CURVES_SEGMENT_COEFF_COUNT> coeffs;
  real_t width;
  real_t x0;
  real_t x;
  real_t expected_t;
  real_t expected_eval;
  real_t tolerance;

  friend auto operator<<(std::ostream& out, const EvaluationTestVector& src)
      -> std::ostream& {
    out << "{.coeffs = {" << src.coeffs[0];
    for (auto coeff = 1U; coeff < CURVES_SEGMENT_COEFF_COUNT; ++coeff) {
      out << ", " << src.coeffs[coeff];
    }
    out << "}, .width = " << src.width << ", .x0 = " << src.x0
        << ", .x = " << src.x << ", .expected_t = " << src.expected_t
        << ", .expected_eval = " << src.expected_eval
        << ", .tolerance = " << src.tolerance << "}";

    return out;
  }
};

struct SegmentEvaluationTest : TestWithParam<EvaluationTestVector> {
  using Sut = SegmentView;

  static auto create_segment() noexcept -> NormalizedSegment {
    SegmentParams construction_params;
    std::ranges::copy(GetParam().coeffs, construction_params.coeffs);
    construction_params.width = GetParam().width;
    return segment::create_segment(construction_params);
  }

  NormalizedSegment segment = create_segment();
  Sut sut{segment};
};

TEST_P(SegmentEvaluationTest, inv_width) {
  const auto actual = sut.inv_width();
  const auto expected = 1.0L / GetParam().width;
  EXPECT_NEAR(actual, expected, 1e-15L);
}

TEST_P(SegmentEvaluationTest, x_to_t) {
  const auto actual = sut.x_to_t(GetParam().x, GetParam().x0);
  const auto expected = GetParam().expected_t;
  EXPECT_NEAR(actual, expected, 1e-12L);
}

TEST_P(SegmentEvaluationTest, eval) {
  const auto t = sut.x_to_t(GetParam().x, GetParam().x0);

  const auto actual = sut.eval(t);
  const auto expected = GetParam().expected_eval;
  EXPECT_NEAR(expected, actual, GetParam().tolerance);
}

const EvaluationTestVector evaluation_test_vectors[] = {
    // Arbitrary segment from viewed in desmos. Expected calculated literally
    // using explicit horner's form in wolfram alpha:
    // ((9.5*0.224489795918 + -6.2)*0.224489795918 + 3.1)*0.224489795918 + 0.2
    {
        .coeffs = {9.5L, -6.2L, 3.1L, 0.2L},
        .width = 4.9L,
        .x0 = 1.4L,
        .x = 2.5L,
        .expected_t = 0.224489795918L,
        .expected_eval = 0.690941699461315064301439052969426004L,
        .tolerance = 6.6e-13L,
    },

    // Segment with denormal coefficient.
    {
        // Coeff[3] is 1.0e-7, which is approx 2^-23.
        // This must trigger the denormal path, a shift of 63.
        .coeffs = {0.0L, 0.0L, 0.0L, 1.0e-7L},
        .width = 1.0L,
        .x0 = 0.0L,
        .x = 0.5L,
        .expected_t = 0.5L,
        .expected_eval = 1.0e-7L,  // Same constant term
        .tolerance = 1.2e-15L,     // Should be exact or extremely close
    },

    // Segment with zero coeffficient.
    {
        .coeffs = {0.0L, 0.0L, 0.0L, 0.0L},
        .width = 10.0L,
        .x0 = 0.0L,
        .x = 5.0L,
        .expected_t = 0.5L,
        .expected_eval = 0.0L,
        .tolerance = 0.0L,
    },

    // Segment with negative zero coeffficient.
    {
        .coeffs = {-0.0L, 0.0L, 0.0L, 0.0L},
        .width = 10.0L,
        .x0 = 0.0L,
        .x = 5.0L,
        .expected_t = 0.5L,
        .expected_eval = 0.0L,
        .tolerance = 0.0L,
    },

    {
        // Coeff[2] (c) is small but positive.
        // We want to verify we aren't losing the bottom bit.
        // Let's use a number that has a specific bit pattern ending at bit 46.
        // 1.5 * 2^-10 = 0.00146484375
        // This should pack perfectly.
        .coeffs = {0.0L, 0.0L, 1.4210854715202004e-14L, 0.0L},  // 2^-46
        .width = 1.0L,
        .x0 = 0.0L,
        .x = 0.5L,
        .expected_t = 0.5L,
        // Expected Eval: coeff[2] * t = 2^-46 * 0.5 = 2^-47
        .expected_eval = 7.105427357601002e-15L,
        // Tolerance: Should be exact (machine epsilon level)
        .tolerance = 1e-20L,
    },

    {
        // Coeff[3] (d) is just below the Normal/Denormal boundary for Unsigned.
        // 2^-17 is approx 7.629e-6. Let's try 7.0e-6.
        // This forces the packer to use Shift 62 and NO implicit bit.
        .coeffs = {0.0L, 0.0L, 0.0L, 7.0e-6L},
        .width = 1.0L,
        .x0 = 0.0L,
        .x = 0.5L,
        .expected_t = 0.5L,
        // Result is just d (constant term)
        .expected_eval = 7.0e-6L,
        .tolerance = 9.1e-17L,
    },

};

INSTANTIATE_TEST_SUITE_P(evaluation_test_vectors, SegmentEvaluationTest,
                         ValuesIn(evaluation_test_vectors));

}  // namespace
}  // namespace curves::segment
