// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/abs.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/limits.hpp>
#include <crv/quadrature/construction/adaptive_integrator.hpp>
#include <crv/quadrature/integral.hpp>
#include <crv/quadrature/rules.hpp>
#include <crv/test/test.hpp>
#include <array>
#include <cmath>
#include <functional>
#include <ostream>

namespace crv::quadrature::construction {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// hybrid tolerance check: |expected - actual| <= max(abs_floor, rel_tol * max(|expected|, |actual|))
// --------------------------------------------------------------------------------------------------------------------

template <typename scalar_t>
auto is_close(char const* expected_expression, char const* actual_expression, char const*, char const*,
    scalar_t expected, scalar_t actual, scalar_t rel_tol, scalar_t abs_floor) -> AssertionResult
{
    auto const diff = abs(expected - actual);
    auto const scale = max({abs_floor, rel_tol * abs(expected), rel_tol * abs(actual)});
    if (diff <= scale) return AssertionSuccess();
    return AssertionFailure() << expected_expression << " = " << expected << " vs " << actual_expression << " = "
                              << actual << "  (diff = " << diff << ", allowed = " << scale << ")";
}

#define EXPECT_CLOSE(expected, actual, rel_tol, abs_floor) \
    EXPECT_PRED_FORMAT4(is_close, expected, actual, rel_tol, abs_floor)

// --------------------------------------------------------------------------------------------------------------------
// shared types
// --------------------------------------------------------------------------------------------------------------------

using scalar_t = float_t;
using rule_t = rules::gauss_kronrod_t<scalar_t>;

struct integrand_t
{
    char const* name;
    std::function<scalar_t(scalar_t)> function;

    auto operator()(scalar_t x) const noexcept -> scalar_t { return function(x); }

    friend auto operator<<(std::ostream& out, integrand_t const& src) -> std::ostream& { return out << src.name; }
};

using integral_t = integral_t<integrand_t, rule_t>;

constexpr auto domain_end = scalar_t{256.0};
constexpr auto depth_limit = int_t{64};
constexpr auto empty_critical_points = std::array<scalar_t, 0>{};

// ====================================================================================================================
// smooth-integrand correctness
//
// Check reported error, analytic F and f at several points, and a reasonable segment count.
// ====================================================================================================================

struct param_t
{
    integrand_t integrand;
    integrand_t expected_antiderivative;
    int_t max_segment_count;

    friend auto operator<<(std::ostream& out, param_t const& src) -> std::ostream&
    {
        return out << "{.integrand = " << src.integrand
                   << ", .expected_antiderivative = " << src.expected_antiderivative << "}";
    }
};

struct quadrature_integration_test_t : TestWithParam<param_t>
{
    integrand_t const& integrand = GetParam().integrand;
    integrand_t const& expected_antiderivative = GetParam().expected_antiderivative;
    int_t const max_segment_count = GetParam().max_segment_count;

    static constexpr auto tolerance = scalar_t{1e-9};

    adaptive_integrator_t<scalar_t> adaptive_integrator{tolerance, depth_limit};
};

TEST_P(quadrature_integration_test_t, matches_analytic_reference)
{
    auto const result = adaptive_integrator(integral_t{integrand, rule_t{}}, domain_end, empty_critical_points);

    EXPECT_LT(result.achieved_error, tolerance);
    EXPECT_LT(result.max_error, tolerance);
    EXPECT_FALSE(result.refinement_limited);
    EXPECT_LE(result.antiderivative.segment_count(), max_segment_count);

    auto const& antiderivative = result.antiderivative;
    auto const input_primals
        = std::array{0.0, 0.1, 0.25, 0.5, 1.0, 5.0, 10.0, 100.0, domain_end / 3.0, domain_end / 2.0, domain_end};
    auto const input_tangent = 2.1;

    for (auto const x : input_primals)
    {
        auto const expected = jet_t<scalar_t>{expected_antiderivative(x), integrand(x) * input_tangent};
        auto const actual = antiderivative(jet_t<scalar_t>{x, input_tangent});
        EXPECT_CLOSE(expected.f, actual.f, 1e-12, 1e-14);
        EXPECT_CLOSE(expected.df, actual.df, 1e-12, 1e-14);
    }
}

param_t const smooth_integrands[] = {
    {{"1", [](scalar_t) { return 1.0; }}, {"x", [](scalar_t x) { return x; }}, 4},
    {{"x", [](scalar_t x) { return x; }}, {"(1/2)x^2", [](scalar_t x) { return x * x / 2.0; }}, 4},
    {{"x^2", [](scalar_t x) { return x * x; }}, {"(1/3)x^3", [](scalar_t x) { return x * x * x / 3.0; }}, 4},
    {{"1/(1+x)", [](scalar_t x) { return 1.0 / (1.0 + x); }}, {"log1p(x)", [](scalar_t x) { return std::log1p(x); }},
        128},
    {{"1/(1+x^2)", [](scalar_t x) { return 1.0 / (1.0 + x * x); }},
        {"atan(x)", [](scalar_t x) { return std::atan(x); }}, 128},
};
INSTANTIATE_TEST_SUITE_P(smooth_integrands, quadrature_integration_test_t, ValuesIn(smooth_integrands));

// ====================================================================================================================
// adaptive refinement stress
// ====================================================================================================================

// localized bump
//
// A narrow gaussian far from the boundaries forces adaptive refinement to find a feature that a coarse uniform grid
// could skip.
TEST(quadrature_integration_adaptive_test_t, localized_bump_triggers_refinement)
{
    constexpr auto sigma = scalar_t{0.5};
    constexpr auto center = scalar_t{128.0};
    constexpr auto tolerance = scalar_t{1e-9};

    auto const bump = integrand_t{"gaussian_bump", [](scalar_t x) {
                                      auto const d = (x - center) / sigma;
                                      return std::exp(-d * d);
                                  }};

    // F(x) = (sigma * sqrt(pi) / 2) * (erf((x - center) / sigma) + erf(center / sigma)) chosen so that
    // F(0) = 0 (erf is odd, and erf(center / sigma) ~= 1 at these values)
    constexpr auto half_sqrt_pi = scalar_t{0.88622692545275794}; // sqrt(pi) / 2
    auto const analytic_antiderivative
        = [](scalar_t x) { return sigma * half_sqrt_pi * (std::erf((x - center) / sigma) + std::erf(center / sigma)); };

    auto integrator = adaptive_integrator_t<scalar_t>{tolerance, depth_limit};
    auto const result = integrator(integral_t{bump, rule_t{}}, domain_end, empty_critical_points);

    EXPECT_LT(result.achieved_error, tolerance);

    // narrow feature should need many more segments than a smooth polynomial
    EXPECT_GT(result.antiderivative.segment_count(), 8);

    auto const& numeric_antiderivative = result.antiderivative;
    for (auto const x : std::array{0.0, 64.0, 120.0, 127.5, 128.0, 128.5, 136.0, 200.0, domain_end})
    {
        EXPECT_CLOSE(analytic_antiderivative(x), numeric_antiderivative(x), 1e-9, 1e-12);
    }
}

// absolute-value kink
//
// Supplying the kink as a critical point splits the function into two linear pieces, which gk15 integrates exactly.
TEST(quadrature_integration_adaptive_test_t, critical_point_tames_kink)
{
    constexpr auto kink_location = scalar_t{3.0};
    constexpr auto tolerance = scalar_t{1e-9};

    auto const kink = integrand_t{"abs(x-3)", [](scalar_t x) { return abs(x - kink_location); }};

    // F(x) on [0, max]:
    //   x <= 3 : 3x - x^2/2
    //   x >  3 : 9/2 + (x-3)^2/2
    auto const analytic_antiderivative = [](scalar_t x) {
        return x <= kink_location ? kink_location * x - x * x / scalar_t{2.0}
                                  : (kink_location * kink_location) / scalar_t{2.0}
                + (x - kink_location) * (x - kink_location) / scalar_t{2.0};
    };

    auto blind = adaptive_integrator_t<scalar_t>{tolerance, depth_limit};
    auto guided = adaptive_integrator_t<scalar_t>{tolerance, depth_limit};

    auto const blind_result = blind(integral_t{kink, rule_t{}}, domain_end, empty_critical_points);
    auto const guided_result = guided(integral_t{kink, rule_t{}}, domain_end, std::array{kink_location});

    EXPECT_LT(blind_result.achieved_error, tolerance);
    EXPECT_LT(guided_result.achieved_error, tolerance);

    for (auto const x : std::array{0.0, 1.0, 3.0, 5.0, 100.0, domain_end})
    {
        EXPECT_CLOSE(analytic_antiderivative(x), blind_result.antiderivative(x), 1e-9, 1e-12);
        EXPECT_CLOSE(analytic_antiderivative(x), guided_result.antiderivative(x), 1e-9, 1e-12);
    }

    // guided path is exact; blind path must refine around the kink
    EXPECT_LT(guided_result.antiderivative.segment_count(), blind_result.antiderivative.segment_count());
}

TEST(quadrature_integration_adaptive_test_t, structural_refinement_limit_returns_best_effort_with_diagnostic)
{
    auto const difficult = integrand_t{"x^40", [](scalar_t x) { return std::pow(x, 40.0); }};
    constexpr auto tolerance = scalar_t{1e-18};
    constexpr auto restrictive_depth_limit = int_t{0};

    auto integrator = adaptive_integrator_t<scalar_t>{tolerance, restrictive_depth_limit};
    auto const result = integrator(integral_t{difficult, rule_t{}}, scalar_t{1.0}, empty_critical_points);

    EXPECT_TRUE(result.refinement_limited);
    EXPECT_GT(result.achieved_error, tolerance);
    EXPECT_EQ(result.antiderivative.segment_count(), 1);
    EXPECT_NEAR(result.antiderivative(1.0), 1.0 / 41.0, 1e-8);
}

// ====================================================================================================================
// invariants under parameter changes
// ====================================================================================================================

// changing tolerances
//
// Tighter tolerance should not increase achieved error or reduce segment count.
TEST(quadrature_integration_invariant_test_t, tighter_tolerance_shrinks_error)
{
    auto const integrand = integrand_t{"1/(1+x^2)", [](scalar_t x) { return 1.0 / (1.0 + x * x); }};

    constexpr auto tolerances = std::array{scalar_t{1e-6}, scalar_t{1e-9}, scalar_t{1e-12}};

    auto prev_error = std::numeric_limits<scalar_t>::infinity();
    auto prev_segments = int_t{0};

    for (auto const tol : tolerances)
    {
        auto integrator = adaptive_integrator_t<scalar_t>{tol, depth_limit};
        auto const result = integrator(integral_t{integrand, rule_t{}}, domain_end, empty_critical_points);

        EXPECT_LT(result.achieved_error, tol);
        EXPECT_LE(result.achieved_error, prev_error);
        EXPECT_GE(result.antiderivative.segment_count(), prev_segments);

        prev_error = result.achieved_error;
        prev_segments = result.antiderivative.segment_count();
    }
}

// critical points only change layout on smooth curves
//
// Extra critical points may change segmentation, but not the integral. This also checks proportional tolerance
// allocation across seeded segments.
TEST(quadrature_integration_invariant_test_t, critical_points_do_not_bias_smooth_result)
{
    auto const integrand = integrand_t{"1/(1+x^2)", [](scalar_t x) { return 1.0 / (1.0 + x * x); }};
    constexpr auto tolerance = scalar_t{1e-12};

    auto bare = adaptive_integrator_t<scalar_t>{tolerance, depth_limit};
    auto split = adaptive_integrator_t<scalar_t>{tolerance, depth_limit};

    auto const bare_result = bare(integral_t{integrand, rule_t{}}, domain_end, empty_critical_points);
    auto const split_result = split(integral_t{integrand, rule_t{}}, domain_end, std::array{32.0, 64.0, 128.0});

    auto const& bare_antiderivative = bare_result.antiderivative;
    auto const& split_antiderivative = split_result.antiderivative;

    for (auto const x : std::array{0.0, 1.0, 32.0, 64.0, 128.0, 200.0, domain_end})
    {
        EXPECT_CLOSE(bare_antiderivative(x), split_antiderivative(x), 1e-10, 1e-12);
    }
}

} // namespace
} // namespace crv::quadrature::construction
