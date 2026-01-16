// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "cubic.hpp"
#include <curves/testing/test.hpp>
#include <ostream>
#include <string>

namespace curves::cubic {
namespace {

using Jet = math::Jet<real_t>;

// ----------------------------------------------------------------------------
// Monomial Form
// ----------------------------------------------------------------------------

struct MonomialTestVector {
  std::string description;
  Monomial monomial;
  real_t t;
  real_t expected_result;
  real_t tolerance = 1e-10;

  friend auto operator<<(std::ostream& out, const MonomialTestVector& src)
      -> std::ostream& {
    return out << "{.description = " << src.description
               << ", .monomial = " << src.monomial << ", .t = " << src.t
               << ", .expected_result = " << src.expected_result
               << ", .tolerance = " << src.tolerance << "}";
  }
};

struct CubicMonomialTest : TestWithParam<MonomialTestVector> {
  const Monomial& monomial = GetParam().monomial;
  const real_t t = GetParam().t;
  const real_t expected_result = GetParam().expected_result;
  const real_t tolerance = GetParam().tolerance;
};

TEST_P(CubicMonomialTest, Eval) {
  const auto acutal_result = monomial(t);

  EXPECT_NEAR(expected_result, acutal_result, tolerance);
}

const MonomialTestVector monomial_test_vectors[] = {
    // Basis Functions
    {"Basis 1, constant", {0, 0, 0, 1}, 0.5, 1.0},
    {"Basis t, linear", {0, 0, 1, 0}, 0.5, 0.5},
    {"Basis t^2, quadratic", {0, 1, 0, 0}, 0.5, 0.25},
    {"Basis t^3, cubic", {1, 0, 0, 0}, 0.5, 0.125},

    // Nominal Cases
    {"t = 0.25", {3, 5, 7, 11}, 0.25, 13.109375},
    {"t = 0.33...", {3, 5, 7, 11}, 1.0 / 3, 14.0},
    {"t = 0.5", {3, 5, 7, 11}, 0.5, 16.125},
    {"t = 0.66...", {3, 5, 7, 11}, 2.0 / 3, 18.77777777777778},
    {"t = 0.75", {3, 5, 7, 11}, 0.75, 20.328125},

    // Edge Cases
    {"t < 0", {3, 5, 7, 11}, -0.5, 8.375},
    {"t = 0", {3, 5, 7, 11}, 0.0, 11.0},  // just coeff d
    {"t = 1", {3, 5, 7, 11}, 1.0, 26.0},  // sum of coefficients
    {"t > 1", {3, 5, 7, 11}, 1.5, 42.875},
};
INSTANTIATE_TEST_SUITE_P(monomial_test_vectors, CubicMonomialTest,
                         ValuesIn(monomial_test_vectors));

// ----------------------------------------------------------------------------
// Hermite Form
// ----------------------------------------------------------------------------

struct HermiteTestVector {
  std::string description;
  Jet left;
  Jet right;
  real_t segment_width;
  Monomial expected_monomial;
  real_t tolerance = 1e-10;

  friend auto operator<<(std::ostream& out, const HermiteTestVector& src)
      -> std::ostream& {
    return out << "{.description = " << src.description
               << ", .left = " << src.left << ", .right = " << src.right
               << ", .segment_width = " << src.segment_width
               << ", .expected_monomial = " << src.expected_monomial
               << ", .tolerance = " << src.tolerance << "}";
  }
};

// hermite_tests[]
struct CubicHermiteTest : TestWithParam<HermiteTestVector> {
  const Jet& left = GetParam().left;
  const Jet& right = GetParam().right;
  const real_t segment_width = GetParam().segment_width;
  const Monomial& expected_monomial = GetParam().expected_monomial;
  const real_t tolerance = GetParam().tolerance;
};

TEST_P(CubicHermiteTest, Eval) {
  const auto acutal_result = hermite_to_monomial(left, right, segment_width);

  EXPECT_NEAR(expected_monomial.coeffs[0], acutal_result.coeffs[0], tolerance);
  EXPECT_NEAR(expected_monomial.coeffs[1], acutal_result.coeffs[1], tolerance);
  EXPECT_NEAR(expected_monomial.coeffs[2], acutal_result.coeffs[2], tolerance);
  EXPECT_NEAR(expected_monomial.coeffs[3], acutal_result.coeffs[3], tolerance);
}

const HermiteTestVector hermite_test_vectors[] = {
    // Constant function: f(x) = 10
    // Expect only the constant term (c[3]) to be set.
    {
        "Constant Value",
        {10.0, 0.0},            // left
        {10.0, 0.0},            // right
        1.0,                    // width
        {0.0, 0.0, 0.0, 10.0},  // 0t^3 + 0t^2 + 0t + 10
    },

    // Linear ramp: f(x) = 2x over [0, 2]
    // Expects slope (2.0) is correctly scaled by width (2.0) to dy/dt = 4.0.
    // Left: (0, 2), right: (4, 2), width: 2.
    {
        "Linear Ramp",
        {0.0, 2.0},
        {4.0, 2.0},
        2.0,
        {0.0, 0.0, 4.0, 0.0},  // 0t^3 + 0t^2 + 4t + 0
    },

    // Standard Cubic Easing Function, Smoothstep: f(t) = 3t^2 - 2t^3
    // Normalized interval [0, 1], so width=1.
    // Slopes at endpoints are 0.
    {
        "Standard Smoothstep",
        {0.0, 0.0},
        {1.0, 0.0},
        1.0,
        {-2.0, 3.0, 0.0, 0.0},  // -2t^3 + 3t^2 + 0t + 0
    },

    // Parabola: f(x) = x^2 over [0, 2]
    // Left: x=0, y=0, y'=0, right: x=2, y=4, y'=4, width: 2.
    // Expect: 4t^2 (since at t=1/x=2, val=4).
    {
        "Parabola",
        {0.0, 0.0},
        {4.0, 4.0},
        2.0,
        {0.0, 4.0, 0.0, 0.0},  // 0t^3 + 4t^2 + 0t + 0
    },

    // Arbitrary Ease-Out: f(t) = -10t^3 + 10t^2 + 10t
    // Strong start slope, flat end.
    // Start: 0, slope 10, end: 10, slope 0, width 1.
    {
        "Arbitrary Ease-Out",
        {0.0, 10.0},
        {10.0, 0.0},
        1.0,
        {-10.0, 10.0, 10.0, 0.0},
    },
};
INSTANTIATE_TEST_SUITE_P(hermite_test_vectors, CubicHermiteTest,
                         ValuesIn(hermite_test_vectors));

}  // namespace
}  // namespace curves::cubic
