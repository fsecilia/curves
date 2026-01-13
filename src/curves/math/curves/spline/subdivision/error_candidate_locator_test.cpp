// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "error_candidate_locator.hpp"
#include <curves/testing/test.hpp>
#include <cmath>
#include <ostream>
#include <string>

namespace curves {
namespace {

using Scalar = double;
using Sut = ErrorCandidateLocator<Scalar>;
using Result = Sut::Result;

struct ErrorCandidateLocatorTestVector {
  std::string description;
  cubic::Monomial<Scalar> monomial;
  Result expected_result;
  Scalar tolerance = 1e-10;

  friend auto operator<<(std::ostream& out,
                         const ErrorCandidateLocatorTestVector& src)
      -> std::ostream& {
    return out << "{.description = " << src.description
               << ", .monomial = " << src.monomial
               << ", .expected_result = " << src.expected_result
               << ", .tolerance = " << src.tolerance << "}";
  }
};

struct CubicErrorCandidateLocatorTest
    : TestWithParam<ErrorCandidateLocatorTestVector> {
  const cubic::Monomial<Scalar>& monomial = GetParam().monomial;
  const Result expected_result = GetParam().expected_result;
  const Scalar tolerance = GetParam().tolerance;

  Sut sut;
};

TEST_P(CubicErrorCandidateLocatorTest, Call) {
  const auto actual_result = sut(monomial);

  ASSERT_EQ(expected_result.size(), actual_result.size());
  for (auto candidate = 0u; candidate < expected_result.size(); ++candidate) {
    EXPECT_NEAR(expected_result[candidate], actual_result[candidate], tolerance)
        << "Mismatch at index " << candidate;
  }
}

// Global constants for test clarity
const auto sqrt_3 = Scalar(std::sqrt(3.0L));
const auto sqrt_7 = Scalar(std::sqrt(7.0L));
const auto sqrt_12 = Scalar(std::sqrt(12.0L));
const auto sqrt_19 = Scalar(std::sqrt(19.0L));

/*
    Helper to create a monomial. We only care about a and b for these tests.
    c and d are initialized to distinct values to ensure the SUT ignores them.
*/
auto make_monomial(Scalar a, Scalar b) noexcept -> cubic::Monomial<Scalar> {
  return {{a, b, Scalar{100.0}, Scalar{-50.0}}};
}

/*
  Simplifies initializing fixed array with variable number of arguments.
*/
auto make_candidates(std::initializer_list<Scalar> inputs) noexcept -> Result {
  return Result{inputs};
}

const ErrorCandidateLocatorTestVector test_vectors[] = {

    // ------------------------------------------------------------------------
    // Standard Cubic Behavior (Full Quadratic Derivative)
    // ------------------------------------------------------------------------

    {
        .description = "standard_cubic_all_candidates_valid",
        // a=1, b=-1.5. Roots at ~0.21, ~0.79, Inflection at 0.5.
        // Verifies the algorithm finds both parallel points and the inflection
        // point.
        .monomial = make_monomial(1.0, -1.5),
        .expected_result =
            make_candidates({(3.0 - sqrt_3) / 6.0, (3.0 + sqrt_3) / 6.0, 0.5}),
    },
    {
        .description = "standard_cubic_negative_a_valid",
        // a=-1, b=1.5. Verifies behavior with inverted curve shape.
        // Result order: t1 (parallel), t2 (parallel), inflection.
        .monomial = make_monomial(-1.0, 1.5),
        .expected_result =
            make_candidates({(3.0 + sqrt_3) / 6.0, (3.0 - sqrt_3) / 6.0, 0.5}),
    },
    {
        .description = "symmetric_inflection_at_midpoint",
        // a=2, b=-3. Inflection exactly at 0.5.
        .monomial = make_monomial(2.0, -3.0),
        .expected_result = make_candidates(
            {(6.0 - sqrt_12) / 12.0, (6.0 + sqrt_12) / 12.0, 0.5}),
    },
    {
        .description = "mixed_signs_one_parallel_filtered_by_0_1_bounds",
        // a=-1, b=2. One parallel point is > 1.0 (filtered).
        // t_valid = 1/3, t_inflection = 2/3.
        .monomial = make_monomial(-1.0, 2.0),
        .expected_result = make_candidates({1.0 / 3.0, 2.0 / 3.0}),
    },

    {
        .description = "asymmetric_inflection_at_quarter_point",
        // a=4, b=-3 -> Inflection at t = -(-3)/(3*4) = 0.25.
        // Parallel points roots: 12t^2 - 6t - 1 = 0
        //   t = (3 ± sqrt(21)) / 12
        //   t1 ≈ -0.13 (filtered < 0)
        //   t2 ≈ 0.632 (valid)
        // Result order: t2 (parallel) then inflection
        .monomial = make_monomial(4.0, -3.0),
        .expected_result =
            make_candidates({(3.0 + std::sqrt(21.0)) / 12.0, 0.25}),
    },

    // ------------------------------------------------------------------------
    // Degenerate Cases (Polynomial Order Reduction)
    // ------------------------------------------------------------------------

    {
        .description = "degenerate_cubic_linear_derivative",
        // a ~ 0, b = 1. The derivative is linear (curve is quadratic).
        // |3a| < threshold, |2b| > threshold.
        // Expected single candidate at t = 0.5.
        .monomial = make_monomial(1e-9, 1.0),
        .expected_result = make_candidates({0.5}),
        .tolerance = 1e-7,
    },
    {
        .description = "degenerate_cubic_constant_derivative",
        // a ~ 0, b ~ 0. The derivative is constant (curve is linear).
        // No error extrema exist relative to secant (parallel everywhere or
        // nowhere).
        .monomial = make_monomial(1e-9, 1e-9),
        .expected_result = make_candidates({}),
    },
    {
        .description = "threshold_boundary_just_quadratic",
        // Test exactly above the epsilon threshold for 'is_quadratic'.
        // a = 4e-8 -> |3a| = 1.2e-7 > 1e-7.
        // Validates numerical path selection, not strict accuracy.
        .monomial = make_monomial(4e-8, 1.0),
        .expected_result = make_candidates({0.5}),  // Approximate
        .tolerance = 0.1,
    },

    // ------------------------------------------------------------------------
    // Numerical Stability
    // ------------------------------------------------------------------------

    {
        .description = "large_coefficients_stability",
        // Large inputs should not cause overflow or logic errors in ratios.
        .monomial = make_monomial(1000.0, -1500.0),
        .expected_result =
            make_candidates({(3.0 - sqrt_3) / 6.0, (3.0 + sqrt_3) / 6.0, 0.5}),
    },
    {
        .description = "small_coefficients_stability",
        // Small inputs should not cause underflow in discriminant calculation.
        .monomial = make_monomial(1e-5, -1.5e-5),
        .expected_result =
            make_candidates({(3.0 - sqrt_3) / 6.0, (3.0 + sqrt_3) / 6.0, 0.5}),
        .tolerance = 1e-8,
    },
};
INSTANTIATE_TEST_SUITE_P(test_vectors, CubicErrorCandidateLocatorTest,
                         ValuesIn(test_vectors));

}  // namespace
}  // namespace curves
