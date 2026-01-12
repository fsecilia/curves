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

using Scalar = double;

// ----------------------------------------------------------------------------
// Monomial Form
// ----------------------------------------------------------------------------

struct MonomialTestVector {
  std::string description;
  Monomial<Scalar> monomial;
  Scalar t;
  Scalar expected_result;
  Scalar tolerance = 1e-10;

  friend auto operator<<(std::ostream& out, const MonomialTestVector& src)
      -> std::ostream& {
    return out << "{.description = " << src.description
               << ", .monomial = " << src.monomial << ", .t = " << src.t
               << ", .expected_result = " << src.expected_result
               << ", .tolerance = " << src.tolerance << "}";
  }
};

struct CubicMonomialTest : TestWithParam<MonomialTestVector> {
  const Monomial<Scalar>& monomial = GetParam().monomial;
  const Scalar t = GetParam().t;
  const Scalar expected_result = GetParam().expected_result;
  const Scalar tolerance = GetParam().tolerance;
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

}  // namespace
}  // namespace curves::cubic
