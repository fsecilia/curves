// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "lambert.hpp"
#include <crv/test/test.hpp>
#include <ostream>

namespace crv {
namespace {

using real_t = float_t;

constexpr auto eps = std::numeric_limits<real_t>::epsilon();
constexpr auto max = std::numeric_limits<real_t>::max();
constexpr auto inf = std::numeric_limits<real_t>::infinity();
constexpr auto nan = std::numeric_limits<real_t>::quiet_NaN();

constexpr auto min_boundary_input = -real_t{1} / std::numbers::e_v<real_t>;

//
// boundary tests
//

// final value that should return nan
TEST(lambert_w0_test_boundaries, min)
{
    EXPECT_TRUE(std::isnan(lambert_w0(std::nextafter(min_boundary_input, -inf))));
}

// Just inside the domain, the root sits at -1 + sqrt(2e*ulp) ~= -1 + 1.7e-8. Convergence degrades from cubic to linear,
// taking ~= 16 iterations from the initial guess. This test is here to fail if num_iterations is ever lowered too far.
TEST(lambert_w0_test_boundaries, just_inside_min)
{
    EXPECT_NEAR(-1.0, lambert_w0(std::nextafter(min_boundary_input, real_t{0})), 5e-8);
}

//
// non-finite and extreme inputs
//
// these pin behavior rather than endorse it. W(+inf) = +inf mathematically, and W(x) is
// finite up to x = max, but w * e^w overflows inside the first iteration once
// x*log(x) > max (x ~= 2.6e305) and the result is nan. nan is a visible failure rather than
// a fake value, so it is documented here instead of special-cased. if +inf is ever
// special-cased to return x, this is the test to change with it

TEST(lambert_w0_test_edge_cases, nan_propagates)
{
    EXPECT_TRUE(std::isnan(lambert_w0(nan)));
}

TEST(lambert_w0_test_edge_cases, negative_infinity_is_outside_domain)
{
    EXPECT_TRUE(std::isnan(lambert_w0(-inf)));
}

TEST(lambert_w0_test_edge_cases, positive_infinity_overflows_to_nan)
{
    EXPECT_TRUE(std::isnan(lambert_w0(inf)));
}

TEST(lambert_w0_test_edge_cases, max_overflows_to_nan)
{
    EXPECT_TRUE(std::isnan(lambert_w0(max)));
}

// parameterized test vector
struct vector_t
{
    real_t input;
    real_t expected;

    friend auto operator<<(std::ostream& out, vector_t const& src) -> std::ostream&
    {
        return out << "{input = " << src.input << ", expected = " << src.expected << "}";
    }
};

// base fixture
struct lambert_w0_test_t : TestWithParam<vector_t>
{
    real_t const input = GetParam().input;
    real_t const expected = GetParam().expected;
};

//
// known, exact values
//

struct lambert_w0_test_known_exact_values_t : lambert_w0_test_t
{};

TEST_P(lambert_w0_test_known_exact_values_t, expected)
{
    auto const actual = lambert_w0(input);

    EXPECT_DOUBLE_EQ(expected, actual);
}

vector_t const known_exact_values[] = {
    // W(0) = 0
    {0.0, 0.0},

    // W(1) = Ω, the omega constant
    // covers initial-guess seam at x == 1, where guess switches from x to log(x)
    {1.0, 0.56714329040978384},

    // W(e) = 1
    {std::exp(1.0), 1.0},
};
INSTANTIATE_TEST_SUITE_P(known_exact_values, lambert_w0_test_known_exact_values_t, ValuesIn(known_exact_values));

//
// values requiring tolerances
//

struct lambert_w0_test_approximate_values_t : lambert_w0_test_t
{};

TEST_P(lambert_w0_test_approximate_values_t, expected)
{
    auto const actual = lambert_w0(input);

    EXPECT_NEAR(expected, actual, 2e-8);
}

vector_t const approximate_values[] = {
    // W(-1/e) = -1
    {min_boundary_input, -1.0},

    // W(0) = 0
    {0.0, 0.0},

    // W(-ln(2)/2) = -ln(2)
    {-std::log(2.0) / 2.0, -std::log(2.0)},
};
INSTANTIATE_TEST_SUITE_P(approximate_values, lambert_w0_test_approximate_values_t, ValuesIn(approximate_values));

//
// property testing
//

struct lambert_w0_test_inverse_property_t : TestWithParam<real_t>
{
    real_t const expected = GetParam();
};

TEST_P(lambert_w0_test_inverse_property_t, round_trip)
{
    // W(x) * e^{W(x)} == x
    //
    // Tolerance is relative, conditioned on w. Loop exits at |we^w - x| <= eps*|x|, but at large x that is finer
    // than the w grid can express. 1 ulp of w perturbs we^w by ~= (1 + w)*eps*x, so the achievable residual grows with
    // w. Actual residuals stay under a third of that bound; 4x here is safety margin.,
    auto const w = lambert_w0(expected);
    auto const actual = w * std::exp(w);
    auto const tolerance = 4 * eps * (1 + std::abs(w)) * std::abs(expected);

    EXPECT_NEAR(expected, actual, tolerance);
}

real_t const inverse_property_values[] = {
    1e-5,
    0.5,
    1.0,
    10.0,
    1000.0,
    1e6,

    // regression for overflow in the halley denominator
    //
    // If `difference` is multipled before dividing by `(value_t{2.0} * w1)`, the calc overflows. These values
    // demonstrated the bug.
    1e303,
    1e305,
};
INSTANTIATE_TEST_SUITE_P(inverse_property, lambert_w0_test_inverse_property_t, ValuesIn(inverse_property_values));

} // namespace
} // namespace crv
