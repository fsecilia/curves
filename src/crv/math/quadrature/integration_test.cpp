// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/math/abs.hpp>
#include <crv/math/compensated_accumulator.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/limits.hpp>
#include <crv/math/quadrature/adaptive_integrator.hpp>
#include <crv/math/quadrature/antiderivative.hpp>
#include <crv/math/quadrature/bisector.hpp>
#include <crv/math/quadrature/integral.hpp>
#include <crv/math/quadrature/rules.hpp>
#include <crv/math/quadrature/segment.hpp>
#include <crv/math/quadrature/stack.hpp>
#include <crv/math/quadrature/subdivider.hpp>
#include <crv/ranges.hpp>
#include <crv/test/test.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <ostream>

namespace crv::quadrature {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// hybrid tolerance check: |expected - actual| <= max(abs_floor, rel_tol * max(|expected|, |actual|))
// --------------------------------------------------------------------------------------------------------------------

template <typename real_t>
auto is_close(char const* expected_expression, char const* actual_expression, char const*, char const*, real_t expected,
    real_t actual, real_t rel_tol, real_t abs_floor) -> AssertionResult
{
    auto const diff = abs(expected - actual);
    auto const scale = std::max({abs_floor, rel_tol * abs(expected), rel_tol * abs(actual)});
    if (diff <= scale) return AssertionSuccess();
    return AssertionFailure() << expected_expression << " = " << expected << " vs " << actual_expression << " = "
                              << actual << "  (diff = " << diff << ", allowed = " << scale << ")";
}

#define EXPECT_CLOSE(expected, actual, rel_tol, abs_floor) \
    EXPECT_PRED_FORMAT4(is_close, expected, actual, rel_tol, abs_floor)

// --------------------------------------------------------------------------------------------------------------------
// shared types
// --------------------------------------------------------------------------------------------------------------------

using real_t = float_t;
using rule_t = rules::gauss_kronrod_t<real_t, 15>;

struct integrand_t
{
    char const* name;
    std::function<real_t(real_t)> function;

    auto operator()(real_t x) const noexcept -> real_t { return function(x); }

    friend auto operator<<(std::ostream& out, integrand_t const& src) -> std::ostream& { return out << src.name; }
};

using integral_t = integral_t<integrand_t, rule_t>;

constexpr auto domain_max = real_t{256.0};
constexpr auto depth_limit = int_t{64};

// ====================================================================================================================
// correctness on smooth integrands
//
// For each (integrand, analytic antiderivative), verify that the integrator:
//   - reports achieved_error and max_error strictly below the requested tolerance
//   - matches the analytic F (and its derivative, the original integrand via FToC) at multiple evaluation points
//   - does not blow up the segment budget for that integrand
// ====================================================================================================================

struct param_t
{
    integrand_t integrand;
    integrand_t expected_antiderivative;
    int_t max_segments;

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
    int_t const max_segments = GetParam().max_segments;

    static constexpr auto tolerance = real_t{1e-9};

    adaptive_integrator_t<real_t> adaptive_integrator{tolerance, depth_limit};
};

TEST_P(quadrature_integration_test_t, matches_analytic_reference)
{
    auto const result = adaptive_integrator(integral_t{integrand, rule_t{}}, domain_max);

    EXPECT_LT(result.achieved_error, tolerance);
    EXPECT_LT(result.max_error, tolerance);
    EXPECT_LE(result.antiderivative.segment_count(), max_segments);

    auto const& antiderivative = result.antiderivative;
    auto const inputs
        = std::array{0.0, 0.1, 0.25, 0.5, 1.0, 5.0, 10.0, 100.0, domain_max / 3.0, domain_max / 2.0, domain_max};

    for (auto const x : inputs)
    {
        auto const expected = jet_t<real_t>{expected_antiderivative(x), integrand(x)};
        auto const actual = antiderivative(x);
        EXPECT_CLOSE(expected.f, actual.f, 1e-12, 1e-14);
        EXPECT_CLOSE(expected.df, actual.df, 1e-12, 1e-14);
    }
}

param_t const smooth_integrands[] = {
    {{"1", [](real_t) { return 1.0; }}, {"x", [](real_t x) { return x; }}, 4},
    {{"x", [](real_t x) { return x; }}, {"(1/2)x^2", [](real_t x) { return x * x / 2.0; }}, 4},
    {{"x^2", [](real_t x) { return x * x; }}, {"(1/3)x^3", [](real_t x) { return x * x * x / 3.0; }}, 4},
    {{"1/(1+x)", [](real_t x) { return 1.0 / (1.0 + x); }}, {"log1p(x)", [](real_t x) { return std::log1p(x); }}, 128},
    {{"1/(1+x^2)", [](real_t x) { return 1.0 / (1.0 + x * x); }}, {"atan(x)", [](real_t x) { return std::atan(x); }},
        128},
};
INSTANTIATE_TEST_SUITE_P(smooth_integrands, quadrature_integration_test_t, ValuesIn(smooth_integrands));

// ====================================================================================================================
// adaptive refinement stress
// ====================================================================================================================

// localized bump
//
// This test contains a narrow gaussian bump centered far from the domain boundaries. The integrator must actually find
// and resolve the feature rather than coarse-stepping past it. A uniform-grid integrator with modest spacing would miss
// it entirely.
TEST(quadrature_integration_adaptive_test_t, localized_bump_triggers_refinement)
{
    constexpr auto sigma = real_t{0.5};
    constexpr auto center = real_t{128.0};
    constexpr auto tolerance = real_t{1e-9};

    auto const bump = integrand_t{"gaussian_bump", [](real_t x) {
                                      auto const d = (x - center) / sigma;
                                      return std::exp(-d * d);
                                  }};

    // F(x) = (sigma * sqrt(pi) / 2) * (erf((x - center) / sigma) + erf(center / sigma)) chosen so that
    // F(0) = 0 (erf is odd, and erf(center / sigma) ~= 1 at these values)
    constexpr auto half_sqrt_pi = real_t{0.88622692545275794}; // sqrt(pi) / 2
    auto const analytic_antiderivative
        = [](real_t x) { return sigma * half_sqrt_pi * (std::erf((x - center) / sigma) + std::erf(center / sigma)); };

    auto integrator = adaptive_integrator_t<real_t>{tolerance, depth_limit};
    auto const result = integrator(integral_t{bump, rule_t{}}, domain_max);

    EXPECT_LT(result.achieved_error, tolerance);

    // Feature is a few sigma wide inside a 256-wide domain. Meaningful adaptive refinement must produce many more
    // segments than the 1-4 that a smooth polynomial takes
    EXPECT_GT(result.antiderivative.segment_count(), 8);

    auto const& numeric_antiderivative = result.antiderivative;
    for (auto const x : std::array{0.0, 64.0, 120.0, 127.5, 128.0, 128.5, 136.0, 200.0, domain_max})
    {
        EXPECT_CLOSE(analytic_antiderivative(x), numeric_antiderivative(x).f, 1e-9, 1e-12);
    }
}

// absolute value kink
//
// This test contains an absolute value kink at a known location. Passing the location as a critical point should cut
// subdivision dramatically, since each resulting half becomes linear and is integrated exactly by gauss-kronrod-15
TEST(quadrature_integration_adaptive_test_t, critical_point_tames_kink)
{
    constexpr auto kink_location = real_t{3.0};
    constexpr auto tolerance = real_t{1e-9};

    auto const kink = integrand_t{"abs(x-3)", [](real_t x) { return abs(x - kink_location); }};

    // F(x) on [0, max]:
    //   x <= 3 : 3x - x^2/2
    //   x >  3 : 9/2 + (x-3)^2/2
    auto const analytic_antiderivative = [](real_t x) {
        return x <= kink_location
            ? kink_location * x - x * x / real_t{2.0}
            : (kink_location * kink_location) / real_t{2.0} + (x - kink_location) * (x - kink_location) / real_t{2.0};
    };

    auto blind = adaptive_integrator_t<real_t>{tolerance, depth_limit};
    auto guided = adaptive_integrator_t<real_t>{tolerance, depth_limit};

    auto const blind_result = blind(integral_t{kink, rule_t{}}, domain_max);
    auto const guided_result = guided(integral_t{kink, rule_t{}}, domain_max, std::array{kink_location});

    EXPECT_LT(blind_result.achieved_error, tolerance);
    EXPECT_LT(guided_result.achieved_error, tolerance);

    for (auto const x : std::array{0.0, 1.0, 3.0, 5.0, 100.0, domain_max})
    {
        EXPECT_CLOSE(analytic_antiderivative(x), blind_result.antiderivative(x).f, 1e-9, 1e-12);
        EXPECT_CLOSE(analytic_antiderivative(x), guided_result.antiderivative(x).f, 1e-9, 1e-12);
    }

    // guided path resolves each linear half exactly; blind path has to find the kink via bisection
    EXPECT_LT(guided_result.antiderivative.segment_count(), blind_result.antiderivative.segment_count());
}

// ====================================================================================================================
// invariants under parameter changes
// ====================================================================================================================

// changing tolerances
//
// Tightening the requested tolerance should never increase achieved_error, and should never decrease segment count.
// A regression in the subdivision predicate, like a reversed comparison, would break one or both of these.
TEST(quadrature_integration_invariant_test_t, tighter_tolerance_shrinks_error)
{
    auto const integrand = integrand_t{"1/(1+x^2)", [](real_t x) { return 1.0 / (1.0 + x * x); }};

    constexpr auto tolerances = std::array{real_t{1e-6}, real_t{1e-9}, real_t{1e-12}};

    auto prev_error = infinity<real_t>();
    auto prev_segments = int_t{0};

    for (auto const tol : tolerances)
    {
        auto integrator = adaptive_integrator_t<real_t>{tol, depth_limit};
        auto const result = integrator(integral_t{integrand, rule_t{}}, domain_max);

        EXPECT_LT(result.achieved_error, tol);
        EXPECT_LE(result.achieved_error, prev_error);
        EXPECT_GE(result.antiderivative.segment_count(), prev_segments);

        prev_error = result.achieved_error;
        prev_segments = result.antiderivative.segment_count();
    }
}

// critical points only change layout on smooth curves
//
// Critical points on a smooth integrand reshape the segment layout but must not bias the integrated result. This pins
// stack_seeder_t's proportional tolerance allocation against regressions that would subtly shift accumulated error
// between segments.
TEST(quadrature_integration_invariant_test_t, critical_points_do_not_bias_smooth_result)
{
    auto const integrand = integrand_t{"1/(1+x^2)", [](real_t x) { return 1.0 / (1.0 + x * x); }};
    constexpr auto tolerance = real_t{1e-12};

    auto bare = adaptive_integrator_t<real_t>{tolerance, depth_limit};
    auto split = adaptive_integrator_t<real_t>{tolerance, depth_limit};

    auto const bare_result = bare(integral_t{integrand, rule_t{}}, domain_max);
    auto const split_result = split(integral_t{integrand, rule_t{}}, domain_max, std::array{32.0, 64.0, 128.0});

    auto const& bare_antiderivative = bare_result.antiderivative;
    auto const& split_antiderivative = split_result.antiderivative;

    for (auto const x : std::array{0.0, 1.0, 32.0, 64.0, 128.0, 200.0, domain_max})
    {
        EXPECT_CLOSE(bare_antiderivative(x).f, split_antiderivative(x).f, 1e-10, 1e-12);
    }
}

} // namespace
} // namespace crv::quadrature
