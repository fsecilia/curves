// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <curves/testing/test.hpp>
#include <curves/math/segment_construction.hpp>
#include <curves/math/segment_eval.hpp>

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

  static auto create_segment() noexcept -> curves_normalized_segment {
    SegmentConstructionParams construction_params;
    std::ranges::copy(GetParam().coeffs, construction_params.coeffs);
    construction_params.width = GetParam().width;
    return curves_create_segment(construction_params);
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

  const auto actual = sut.eval(t);
  const auto expected = GetParam().expected_eval;
  EXPECT_NEAR(expected, actual, GetParam().tolerance);
}

const EvaluationTestVector evaluation_test_vectors[] = {
    // Arbitrary segment from viewed in desmos.
    {
        .coeffs = {9.5, -6.2, 3.1, 0.2},
        .width = 4.9,
        .x0 = 1.4,
        .x = 2.5,
        .expected_t = 0.224489795918,
        .expected_eval = 0.6909416994619L,
        .tolerance = 4.2e-14,
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
};

INSTANTIATE_TEST_SUITE_P(evaluation_test_vectors, SegmentEvaluationTest,
                         ValuesIn(evaluation_test_vectors));

}  // namespace
}  // namespace curves::segment
