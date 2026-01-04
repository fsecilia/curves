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
    for (auto coeff = 1; coeff < CURVES_SEGMENT_COEFF_COUNT; ++coeff) {
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

  const auto actual = sut.eval(t);
  const auto expected = GetParam().expected_eval;
  EXPECT_NEAR(expected, actual, GetParam().tolerance);
}

const EvaluationTestVector evaluation_test_vectors[] = {
    // Arbitrary segment from viewed in desmos. Expected calculated literally
    // using explicit horner's form in wolfram alpha:
    // ((9.5*0.224489795918 + -6.2)*0.224489795918 + 3.1)*0.224489795918 + 0.2
    {
        .coeffs = {9.5, -6.2, 3.1, 0.2},
        .width = 4.9,
        .x0 = 1.4,
        .x = 2.5,
        .expected_t = 0.224489795918,
        .expected_eval = 0.690941699461315064301439052969426004L,
        .tolerance = 6.6e-13,
    },

    // Segment with denormal coefficient.
    {
        // Coeff[3] is 1.0e-7, which is approx 2^-23.
        // This must trigger the denormal path, a shift of 63.
        .coeffs = {0.0, 0.0, 0.0, 1.0e-7},
        .width = 1.0,
        .x0 = 0.0,
        .x = 0.5,
        .expected_t = 0.5,
        .expected_eval = 1.0e-7,  // Same constant term
        .tolerance = 1.2e-15,     // Should be exact or extremely close
    },

    // Segment with zero coeffficient.
    {
        .coeffs = {0.0, 0.0, 0.0, 0.0},
        .width = 10.0,
        .x0 = 0.0,
        .x = 5.0,
        .expected_t = 0.5,
        .expected_eval = 0.0,
        .tolerance = 0.0,
    },

    // Segment with negative zero coeffficient.
    {
        .coeffs = {-0.0, 0.0, 0.0, 0.0},
        .width = 10.0,
        .x0 = 0.0,
        .x = 5.0,
        .expected_t = 0.5,
        .expected_eval = 0.0,
        .tolerance = 0.0,
    },

    {
        // Coeff[2] (c) is small but positive.
        // We want to verify we aren't losing the bottom bit.
        // Let's use a number that has a specific bit pattern ending at bit 46.
        // 1.5 * 2^-10 = 0.00146484375
        // This should pack perfectly.
        .coeffs = {0.0, 0.0, 1.4210854715202004e-14, 0.0},  // 2^-46
        .width = 1.0,
        .x0 = 0.0,
        .x = 0.5,
        .expected_t = 0.5,
        // Expected Eval: coeff[2] * t = 2^-46 * 0.5 = 2^-47
        .expected_eval = 7.105427357601002e-15,
        // Tolerance: Should be exact (machine epsilon level)
        .tolerance = 1e-20,
    },

    {
        // Coeff[3] (d) is just below the Normal/Denormal boundary for Unsigned.
        // 2^-17 is approx 7.629e-6. Let's try 7.0e-6.
        // This forces the packer to use Shift 62 and NO implicit bit.
        .coeffs = {0.0, 0.0, 0.0, 7.0e-6},
        .width = 1.0,
        .x0 = 0.0,
        .x = 0.5,
        .expected_t = 0.5,
        // Result is just d (constant term)
        .expected_eval = 7.0e-6,
        .tolerance = 9.1e-17,
    },

};

INSTANTIATE_TEST_SUITE_P(evaluation_test_vectors, SegmentEvaluationTest,
                         ValuesIn(evaluation_test_vectors));

}  // namespace
}  // namespace curves::segment
