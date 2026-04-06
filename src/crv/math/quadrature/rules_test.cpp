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
// Compile-Time Tests
// ====================================================================================================================

constexpr auto near(real_t a, real_t b, real_t tolerance = 1e-12) noexcept -> bool
{
    using crv::abs;
    return abs(a - b) <= tolerance;
}

// --------------------------------------------------------------------------------------------------------------------
// Common Rule Properties
// --------------------------------------------------------------------------------------------------------------------

/// affine transformation of intervals is robust
template <typename rule_t> struct common_domain_tests_t
{
    static constexpr auto rule = rule_t{};

    // constant: f(x) = 5, integral [0, 2] = 10
    static_assert(near(rule(0.0, 2.0, [](real_t) { return 5.0; }), 10.0));

    // reversed bounds: f(x) = x^2, integral [2, 0] = -8/3
    static_assert(near(rule(2.0, 0.0, [](real_t x) { return x * x; }), -8.0 / 3.0));

    // zero-width: f(x) = x^2, integral [1, 1] = 0
    static_assert(near(rule(1.0, 1.0, [](real_t x) { return x * x; }), 0.0));

    // negative bounds: f(x) = x^2, integral [-3, -1] = 26/3
    static_assert(near(rule(-3.0, -1.0, [](real_t x) { return x * x; }), 26.0 / 3.0));

    // large offset: f(x) = x^2, integral [1000, 1002] = 6012008/3
    static_assert(near(rule(1000.0, 1002.0, [](real_t x) { return x * x; }), 6012008.0 / 3.0, 1e-6));

    // tiny interval: f(x) = x^2, integral [0, 0.001] = 1e-9/3
    static_assert(near(rule(0.0, 0.001, [](real_t x) { return x * x; }), 1e-9 / 3.0));
};

/// algorithm respects parity and symmetrical cancellation
template <typename rule_t> struct common_symmetry_tests_t
{
    static constexpr auto rule = rule_t{};

    // odd function, symmetric bounds: f(x) = x^3, integral [-1, 1] = 0
    static_assert(near(rule(-1.0, 1.0, [](real_t x) { return x * x * x; }), 0.0));

    // odd function, asymmetric bounds: f(x) = x^3, integral [1, 3] = 20
    static_assert(near(rule(1.0, 3.0, [](real_t x) { return x * x * x; }), 20.0));
};

/// smooth approximation of non-polynomial (rational) curves
template <typename rule_t> struct common_approximation_tests_t
{
    static constexpr auto rule = rule_t{};

    // f(x) = 1 / x, integral [1, 2] = ln(2) ≈ 0.6931471805599453
    static_assert(near(rule(1.0, 2.0, [](real_t x) { return 1.0 / x; }), 0.6931471805599453, 1e-5));

    // f(x) = 1 / (1 + x^2), integral [0, 1] = arctan(1) = pi/4 ≈ 0.7853981633974483
    static_assert(near(rule(0.0, 1.0, [](real_t x) { return 1.0 / (1.0 + x * x); }), 0.7853981633974483, 1e-5));
};

// explicitly instantiate common tests
template struct common_domain_tests_t<gauss_t<4>>;
template struct common_symmetry_tests_t<gauss_t<4>>;
template struct common_approximation_tests_t<gauss_t<4>>;
template struct common_domain_tests_t<gauss_t<5>>;
template struct common_symmetry_tests_t<gauss_t<5>>;
template struct common_approximation_tests_t<gauss_t<5>>;

// --------------------------------------------------------------------------------------------------------------------
// Rule-Specific Polynomial Precision
// --------------------------------------------------------------------------------------------------------------------

namespace gauss4_tests {

constexpr auto rule = gauss_t<4>{};

// degree 2: f(x) = x^2, integral [0, 2] = 8/3
static_assert(near(rule(0.0, 2.0, [](real_t x) { return x * x; }), 8.0 / 3.0));

// degree 7 (max exact): f(x) = x^7, integral [0, 2] = 256 / 8 = 32.0
static_assert(near(rule(0.0, 2.0,
                        [](real_t x) {
                            auto const x2 = x * x;
                            auto const x4 = x2 * x2;
                            return x4 * x2 * x;
                        }),
                   32.0));

// degree 8 (failure boundary): f(x) = x^8, integral [-1, 1] = 2/9
static_assert(!near(rule(-1.0, 1.0,
                         [](real_t x) {
                             auto const x2 = x * x;
                             auto const x4 = x2 * x2;
                             return x4 * x4;
                         }),
                    2.0 / 9.0));

} // namespace gauss4_tests

// --------------------------------------------------------------------------------------------------------------------

namespace gauss5_tests {

constexpr auto rule = gauss_t<5>{};

// degree 2: f(x) = x^2, integral [0, 2] = 8/3
static_assert(near(rule(0.0, 2.0, [](real_t x) { return x * x; }), 8.0 / 3.0));

// degree 8: f(x) = x^8, integral [-1, 1] = 2/9
static_assert(near(rule(-1.0, 1.0,
                        [](real_t x) {
                            auto const x2 = x * x;
                            auto const x4 = x2 * x2;
                            return x4 * x4;
                        }),
                   2.0 / 9.0));

// degree 9 (max exact): f(x) = x^9, integral [0, 2] = 1024 / 10 = 102.4
static_assert(near(rule(0.0, 2.0,
                        [](real_t x) {
                            auto const x2 = x * x;
                            auto const x4 = x2 * x2;
                            auto const x8 = x4 * x4;
                            return x8 * x;
                        }),
                   102.4));

// degree 10 (failure boundary): f(x) = x^10, integral [-1, 1] = 2/11
static_assert(!near(rule(-1.0, 1.0,
                         [](real_t x) {
                             auto const x2 = x * x;
                             auto const x4 = x2 * x2;
                             auto const x8 = x4 * x4;
                             return x8 * x2;
                         }),
                    2.0 / 11.0));

} // namespace gauss5_tests

// ====================================================================================================================
// Runtime Tests
// ====================================================================================================================

// endpoint singularity: f(x) = ln(x), integral [0, 1] = -1
struct quadrature_rules_endpoint_singularity_test_log_t : Test
{
    static constexpr auto expected = -1.0;
    static auto           integrand(real_t x) noexcept -> real_t { return std::log(x); }
};

TEST_F(quadrature_rules_endpoint_singularity_test_log_t, Gauss4)
{
    EXPECT_NEAR(gauss_t<4>{}(0.0, 1.0, integrand), expected, 5e-2);
}

TEST_F(quadrature_rules_endpoint_singularity_test_log_t, Gauss5)
{
    EXPECT_NEAR(gauss_t<5>{}(0.0, 1.0, integrand), expected, 5e-2);
}

// --------------------------------------------------------------------------------------------------------------------

// endpoint singularity: f(x) = 1/sqrt(x), integral [0, 1] = 2
struct quadrature_rules_endpoint_singularity_test_sqrt_t : Test
{
    static constexpr auto expected = 2.0;
    static auto           integrand(real_t x) noexcept -> real_t { return 1.0 / std::sqrt(x); }
};

TEST_F(quadrature_rules_endpoint_singularity_test_sqrt_t, Gauss4)
{
    EXPECT_NEAR(gauss_t<4>{}(0.0, 1.0, integrand), expected, 2.5e-1);
}

TEST_F(quadrature_rules_endpoint_singularity_test_sqrt_t, Gauss5)
{
    EXPECT_NEAR(gauss_t<5>{}(0.0, 1.0, integrand), expected, 2e-1);
}

// --------------------------------------------------------------------------------------------------------------------

// smooth transcendental: f(x) = e^x, integral [0, 1] = e - 1
struct quadrature_rules_smooth_test_exp_t : Test
{
    static inline auto const expected = std::exp(1.0) - 1.0;

    static auto integrand(real_t x) noexcept -> real_t { return std::exp(x); }
};

TEST_F(quadrature_rules_smooth_test_exp_t, Gauss4)
{
    EXPECT_NEAR(gauss_t<4>{}(0.0, 1.0, integrand), expected, 1e-8);
}

TEST_F(quadrature_rules_smooth_test_exp_t, Gauss5)
{
    EXPECT_NEAR(gauss_t<5>{}(0.0, 1.0, integrand), expected, 1e-12);
}

// --------------------------------------------------------------------------------------------------------------------

// smooth transcendental: f(x) = ln(x), integral [1, 2] = 2*ln(2) - 1
struct quadrature_rules_smooth_test_log_t : Test
{
    static inline auto const expected = 2.0 * std::log(2.0) - 1.0;

    static auto integrand(real_t x) noexcept -> real_t { return std::log(x); }
};

TEST_F(quadrature_rules_smooth_test_log_t, Gauss4)
{
    EXPECT_NEAR(gauss_t<4>{}(1.0, 2.0, integrand), expected, 1e-6);
}

TEST_F(quadrature_rules_smooth_test_log_t, Gauss5)
{
    EXPECT_NEAR(gauss_t<5>{}(1.0, 2.0, integrand), expected, 1e-8);
}

// --------------------------------------------------------------------------------------------------------------------

// oscillatory: f(x) = sin(10x), integral [0, pi] = 0 (5 full periods, symmetric)
struct quadrature_rules_oscillatory_test_sin_t : Test
{
    static constexpr auto expected = 0.0;
    static constexpr auto pi       = std::numbers::pi;

    static auto integrand(real_t x) noexcept -> real_t { return std::sin(10.0 * x); }
};

TEST_F(quadrature_rules_oscillatory_test_sin_t, Gauss4)
{
    EXPECT_NEAR(gauss_t<4>{}(0.0, pi, integrand), expected, 1e-1);
}

TEST_F(quadrature_rules_oscillatory_test_sin_t, Gauss5)
{
    EXPECT_NEAR(gauss_t<5>{}(0.0, pi, integrand), expected, 1e-1);
}

} // namespace
} // namespace crv::quadrature::rules
