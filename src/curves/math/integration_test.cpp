// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "integration.hpp"
#include <curves/testing/test.hpp>
#include <string_view>

namespace curves {
namespace {

struct IntegrationTestVector {
  std::string_view description;
  real_t h;         // Step size
  Jet start;        // Jet at a
  Jet end;          // Jet at b
  real_t expected;  // Analytical integral result
};

class IntegrationTest : public ::testing::TestWithParam<IntegrationTestVector> {
};

TEST_P(IntegrationTest, corrected_trapezoid4) {
  const auto& p = GetParam();

  real_t result = corrected_trapezoid(p.h, p.start, p.end);

  /*
    We use a *very* tight tolerance here because, for cubic and below, this
    method is mathematically exact independent of step size.
  */
  EXPECT_NEAR(result, p.expected, 1e-15) << "Failed on: " << p.description;
}

const IntegrationTestVector IntegrationPolynomialChecks[] = {
    // Constant: f(x) = 2, f'(x) = 0. Int[0,1] = 2.
    {"Constant Function (Horizontal Line)", 1.0, {2.0, 0.0}, {2.0, 0.0}, 2.0},

    // Linear: f(x) = x, f'(x) = 1. Int[0,1] = 0.5.
    {"Linear Function (Slope)", 1.0, {0.0, 1.0}, {1.0, 1.0}, 0.5},

    // Quadratic: f(x) = x^2, f'(x) = 2x. Int[0,1] = 1/3.
    {"Quadratic (Parabola)", 1.0, {0.0, 0.0}, {1.0, 2.0}, 1.0 / 3.0},

    // Cubic: f(x) = x^3, f'(x) = 3x^2. Int[0,1] = 1/4.
    // This is the limit of exact precision for this rule.
    {"Cubic (S-Curve)", 1.0, {0.0, 0.0}, {1.0, 3.0}, 0.25},

    // Scaled Interval: f(x) = x^2 over [0, 2]. Int[0,2] = 8/3.
    // Checks if 'h' is applied correctly.
    {"Quadratic over larger interval [0, 2]",
     2.0,
     {0.0, 0.0},
     {4.0, 4.0},
     8.0 / 3.0},

    // Negative Slope: f(x) = -x, f'(x) = -1. Int[0,1] = -0.5.
    {"Negative Slope", 1.0, {0.0, -1.0}, {-1.0, -1.0}, -0.5},

    // Arbitrary Cubic Polynomial: f(x) = x^3 - 2x^2 + 4x + 1
    // Interval: [0, 3]
    // Exact Integral: 23.25
    // This validates the "Oracle Property": The Corrected Trapezoidal Rule
    // is mathematically exact for cubic splines.
    {
        "Arbitrary Cubic (Oracle Check)",
        3.0,           // h (Step size 0 to 3)
        {1.0, 4.0},    // Start Jet (at x=0)
        {22.0, 19.0},  // End Jet   (at x=3)
        23.25          // Expected Result
    }};

INSTANTIATE_TEST_SUITE_P(
    PolynomialChecks, IntegrationTest,
    ::testing::ValuesIn(IntegrationPolynomialChecks),

    [](const testing::TestParamInfo<IntegrationTestVector>& info) {
      // Replace spaces with underscores for valid GTest names.
      auto s = std::string{info.param.description};
      std::replace_if(
          s.begin(), s.end(), [](char c) { return !isalnum(c); }, '_');
      return s;
    });

}  // namespace
}  // namespace curves
