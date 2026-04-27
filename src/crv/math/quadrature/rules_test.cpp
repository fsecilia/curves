// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "rules.hpp"
#include <crv/math/abs.hpp>
#include <crv/test/test.hpp>
#include <cmath>
#include <numbers>

namespace crv::quadrature::rules {
namespace {

using real_t = float_t;

// ====================================================================================================================
// compile-time tests
// ====================================================================================================================

constexpr auto near(real_t a, real_t b, real_t tolerance = 1e-12) noexcept -> bool
{
    return abs(a - b) <= tolerance;
}

constexpr auto power(real_t base, int exponent) noexcept -> real_t
{
    auto result = real_t{1};
    for (auto i = 0; i < exponent; ++i) { result *= base; }
    return result;
}

constexpr auto rule = gauss_kronrod_t<real_t>{};

// constant: f(x) = 5, integral [0, 2] = 10
static_assert(near(rule.integrate(0.0, 2.0, [](real_t) { return 5.0; }), 10.0));

// reversed bounds: f(x) = x^2, integral [2, 0] = -8/3
static_assert(near(rule.integrate(2.0, 0.0, [](real_t x) { return x * x; }), -8.0 / 3.0));

// zero-width: f(x) = x^2, integral [1, 1] = 0
static_assert(near(rule.integrate(1.0, 1.0, [](real_t x) { return x * x; }), 0.0));

// negative bounds: f(x) = x^2, integral [-3, -1] = 26/3
static_assert(near(rule.integrate(-3.0, -1.0, [](real_t x) { return x * x; }), 26.0 / 3.0));

// large offset: f(x) = x^2, integral [1000, 1002] = 6012008/3
static_assert(near(rule.integrate(1000.0, 1002.0, [](real_t x) { return x * x; }), 6012008.0 / 3.0, 1e-6));

// tiny interval: f(x) = x^2, integral [0, 0.001] = 1e-9/3
static_assert(near(rule.integrate(0.0, 0.001, [](real_t x) { return x * x; }), 1e-9 / 3.0));

// odd function, symmetric bounds: f(x) = x^3, integral [-1, 1] = 0
static_assert(near(rule.integrate(-1.0, 1.0, [](real_t x) { return power(x, 3); }), 0.0));

// odd function, asymmetric bounds: f(x) = x^3, integral [1, 3] = 20
static_assert(near(rule.integrate(1.0, 3.0, [](real_t x) { return power(x, 3); }), 20.0));

// rational to transcendental, f(x) = 1 / x, integral [1, 2] = ln(2) ≈ 0.6931471805599453
static_assert(near(rule.integrate(1.0, 2.0, [](real_t x) { return 1.0 / x; }), 0.6931471805599453, 1e-5));

// rational to transcendental, f(x) = 1 / (1 + x^2), integral [0, 1] = arctan(1) = pi/4 ≈ 0.7853981633974483
static_assert(near(rule.integrate(0.0, 1.0, [](real_t x) { return 1.0 / (1.0 + x * x); }), 0.7853981633974483, 1e-5));

// --------------------------------------------------------------------------------------------------------------------
// precision
// --------------------------------------------------------------------------------------------------------------------

// total exact
//
// G7 is exact up to degree 13 (2n - 1). K15 is also exact.
// Integral of x^13 on [0, 1] = 1/14. Both rules agree exactly, so error estimate is ~0.
constexpr auto total_exact = rule.estimate(0.0, 1.0, [](real_t x) { return power(x, 13); });
static_assert(near(total_exact.sum, 1.0 / 14.0));
static_assert(near(total_exact.error, 0.0, 1e-14));

// K15 exact
//
// K15 is exact up to degree 23 (3n + 2). G7 fails.
// Integral of x^23 on [0, 1] = 1/24. K15 computes the exact value, but the error estimate is > 0.
constexpr auto k15_exact = rule.estimate(0.0, 1.0, [](real_t x) { return power(x, 23); });
static_assert(near(k15_exact.sum, 1.0 / 24.0));
static_assert(k15_exact.error > 1e-8);

// demonstrated failure
//
// K15 exactness mathematically ends at degree 23. However, for degree 24, the theoretical truncation error is so
// infinitesimally small that it hides entirely beneath 64-bit floating-point epsilon. Since we cannot strictly test the
// true boundary without arbitrary precision, we use degree 40 to force a truncation error large enough to be detected.
constexpr auto demonstrated_failure = rule.estimate(0.0, 1.0, [](real_t x) { return power(x, 40); });
static_assert(abs(demonstrated_failure.sum - (1.0 / 41.0)) > std::numeric_limits<real_t>::epsilon());
static_assert(demonstrated_failure.error > 1e-8);

// ====================================================================================================================
// runtime tests
// ====================================================================================================================

// endpoint singularity: f(x) = ln(x), integral [0, 1] = -1
TEST(quadrature_rules_test, endpoint_singularity_log)
{
    auto constexpr expected = -1.0;
    auto const actual = rule.integrate(0.0, 1.0, [](real_t x) { return std::log(x); });
    EXPECT_NEAR(actual, expected, 5e-2);
}

// endpoint singularity: f(x) = 1/sqrt(x), integral [0, 1] = 2
TEST(quadrature_rules_test, endpoint_singularity_sqrt)
{
    auto constexpr expected = 2.0;
    auto const actual = rule.integrate(0.0, 1.0, [](real_t x) { return 1.0 / std::sqrt(x); });
    EXPECT_NEAR(actual, expected, 5e-2);
}

// fractional power law: f(x) = x^1.5, integral [0, 1] = 0.4
TEST(quadrature_rules_test, fractional_power_law)
{
    auto constexpr expected = 0.4;
    auto const actual = rule.estimate(0.0, 1.0, [](real_t x) { return std::pow(x, 1.5); });

    EXPECT_NEAR(actual.sum, expected, 1e-7);
    EXPECT_GT(actual.error, 0.0);
    EXPECT_LT(actual.error, 1e-5);
    EXPECT_LE(crv::abs(expected - actual.sum), actual.error);
}

// smooth transcendental: f(x) = ln(x), integral [1, 2] = 2*ln(2) - 1
TEST(quadrature_rules_test, smooth_transcendental_log)
{
    auto const expected = 2.0 * std::log(2.0) - 1.0;
    auto const actual = rule.estimate(1.0, 2.0, [](real_t x) { return std::log(x); });

    EXPECT_NEAR(actual.sum, expected, 1e-14);
    EXPECT_LE(crv::abs(expected - actual.sum), actual.error);
}

// oscillatory: f(x) = sin(10x), integral [0, pi] = 0 (5 full periods, symmetric)
TEST(quadrature_rules_test, oscillatory_sin)
{
    auto constexpr expected = 0.0;
    auto const actual = rule.estimate(0.0, std::numbers::pi, [](real_t x) { return std::sin(10.0 * x); });

    EXPECT_NEAR(actual.sum, expected, 1e-14);
    EXPECT_LE(crv::abs(expected - actual.sum), actual.error);
}

} // namespace
} // namespace crv::quadrature::rules
