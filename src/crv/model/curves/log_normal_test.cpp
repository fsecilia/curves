// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "log_normal.hpp"
#include <crv/model/curves/test.hpp>
#include <crv/test/test.hpp>
#include <cmath>
#include <complex>
#include <limits>
#include <numbers>

namespace crv::model::curves {

template <std::floating_point real_t>
auto operator<<(std::ostream& out, log_normal_t::params_t<real_t> const& src) -> std::ostream&
{
    return out << "{baseline = " << src.baseline << ", limit = " << src.limit << ", mu = " << src.mu
               << ", sigma = " << src.sigma << "}";
}

namespace {

using real_t = float_t;
using params_t = log_normal_t::params_t<real_t>;
using evaluator_t = log_normal_t::evaluator_t<real_t>;
using complex_evaluator_t = log_normal_t::evaluator_t<std::complex<real_t>>;

auto const df = real_t{1.3};

//
// math
//

// common test vector for multiple tests
struct vector_t
{
    std::string_view name;
    params_t params;

    friend auto operator<<(std::ostream& out, vector_t const& src) -> std::ostream&
    {
        return out << "{.params = " << src.params << "}";
    }
};

// sweep table for multiple tests
//
// mu and sigma are swept independently over the box mu in [-7, 7], sigma in [0.1, 10], plus both corners.
vector_t const sweep_vectors[] = {
    {"standard", {0.0, 1.0, 1.0, 0.5}},
    {"shifted", {2.0, 4.0, 0.0, 1.0}},
    {"narrow", {0.5, 1.5, 2.0, 0.1}},
    {"wide", {0.0, 1.0, 5.0, 2.0}},

    // location and shape extremes, swept independently
    {"mu_lo", {0.0, 1.0, -7.0, 1.0}},
    {"mu_hi", {0.0, 1.0, 7.0, 1.0}},
    {"sigma_lo", {0.0, 1.0, 0.0, 0.1}},
    {"sigma_hi", {0.0, 1.0, 0.0, 10.0}},

    // corners
    {"lo_all", {0.0, 1.0, -7.0, 0.1}},
    {"hi_all", {0.0, 1.0, 7.0, 10.0}},

    // offset dominates delta; catches relative/absolute mixups in the affine map
    {"offset", {10.0, 11.0, 2.0, 0.5}},
};

struct model_curves_log_normal_math_test_t : TestWithParam<vector_t>
{
    params_t const& params = GetParam().params;
    real_t delta = params.limit - params.baseline;

    static constexpr auto inv_sqrt_pi = std::numbers::inv_sqrtpi_v<real_t>;
    static constexpr auto sqrt2 = std::numbers::sqrt2_v<real_t>;

    static constexpr auto tolerance = real_t{1e-12};

    // multipliers of the median x0 = exp(mu), giving x well to either side; all stay clear of the origin threshold
    static constexpr real_t multipliers[] = {0.02, 0.1, 0.3, 0.6, 0.9, 1.1, 2.0, 5.0, 50.0, 1e6};

    // x = exp(mu) is the median of the underlying log-normal
    auto x_median() const noexcept -> real_t { return std::exp(params.mu); }

    // f'(x0) = delta*inv_sqrt_pi/(x0*sigma*sqrt2): the curve's own slope scale. f' carries units of 1/x, so no absolute
    // tolerance fits every row; saturation checks on f' must be relative to this.
    auto median_slope() const noexcept -> real_t { return delta * inv_sqrt_pi / (x_median() * params.sigma * sqrt2); }

    // log(x_far) = mu + 8*sigma => z = 8/sqrt2 = 5.66. both residuals are scale-free and row-independent:
    //
    //     value: (f(x_far) - limit)/delta = erfc(5.66)/2           < 1e-15
    //     slope: f'(x_far)/f'(x0)         = exp(-32)*exp(-8*sigma) < 1.3e-14 for all sigma > 0
    //
    auto x_far() const noexcept -> real_t { return std::exp(params.mu + 8 * params.sigma); }

    // largest x strictly below the saturation branch's exclusive threshold
    auto x_below_threshold() const noexcept -> real_t
    {
        return std::nextafter(static_cast<real_t>(evaluator_t::x_origin_saturation_threshold), real_t{0});
    }

    evaluator_t const sut{params};
    complex_evaluator_t const complex_sut{params};
};

//
// median
//
// At x0 = exp(mu), z = 0, so:
//
//     f(x0)  = baseline + delta/2
//     f'(x0) = delta/(x0*sigma*sqrt(2*pi)) = delta*inv_sqrt_pi/(x0*sigma*sqrt2)
//

TEST_P(model_curves_log_normal_math_test_t, value_is_half_scale_at_median)
{
    EXPECT_NEAR(params.baseline + 0.5 * delta, sut(x_median()), tolerance);
}

TEST_P(model_curves_log_normal_math_test_t, first_derivative_matches_closed_form_at_median)
{
    auto const y = sut(jet_t{x_median(), df});

    EXPECT_NEAR(params.baseline + 0.5 * delta, y.f, tolerance);
    EXPECT_LT(rel_error(y.df, df * delta * inv_sqrt_pi / (x_median() * params.sigma * sqrt2)), tolerance);
}

TEST_P(model_curves_log_normal_math_test_t, zero_tangent_propagates_zero_velocity)
{
    auto const y = sut(jet_t{x_median(), 0.0});

    EXPECT_NEAR(params.baseline + 0.5 * delta, y.f, tolerance);
    EXPECT_EQ(0.0, y.df);
}

//
// origin threshold
//

TEST_P(model_curves_log_normal_math_test_t, scalar_saturates_below_origin_threshold)
{
    EXPECT_EQ(params.baseline, sut(0.0));
    EXPECT_EQ(params.baseline, sut(x_below_threshold()));
}

TEST_P(model_curves_log_normal_math_test_t, jet_saturates_below_origin_threshold)
{
    auto const at_zero = sut(jet_t{0.0, df});
    EXPECT_EQ(params.baseline, at_zero.f);
    EXPECT_EQ(0.0, at_zero.df);

    auto const below = sut(jet_t{x_below_threshold(), df});
    EXPECT_EQ(params.baseline, below.f);
    EXPECT_EQ(0.0, below.df);
}

//
// asymptote
//

TEST_P(model_curves_log_normal_math_test_t, scalar_saturates_at_asymptote)
{
    EXPECT_NEAR(params.limit, sut(x_far()), tolerance);
}

TEST_P(model_curves_log_normal_math_test_t, jet_saturates_at_asymptote)
{
    auto const y = sut(jet_t{x_far(), df});

    EXPECT_NEAR(params.limit, y.f, tolerance * delta);

    // relative to the curve's own slope scale: y.df/(df*f'(x0)) = exp(-32 - 8*sigma) <= 1.3e-14 for every row
    EXPECT_NEAR(0.0, y.df, tolerance * df * median_slope());
}

//
// cross-path consistency
//
// Sweeps x across the median for every param row.
//

// jet branch applies same affine map as scalar branch
TEST_P(model_curves_log_normal_math_test_t, scalar_matches_jet_primal)
{
    for (auto const multiplier : multipliers)
    {
        auto const x = x_median() * multiplier;
        EXPECT_LT(rel_error(sut(x), sut(jet_t{x, df}).f), tolerance);
    }
}

// check chain-rule plumbing via complex step
TEST_P(model_curves_log_normal_math_test_t, first_derivative_matches_complex_step_of_value)
{
    for (auto const multiplier : multipliers)
    {
        auto const x = x_median() * multiplier;
        auto const expected = df * complex_step_derivative([&](auto z) { return complex_sut(z); }, x);

        EXPECT_LT(rel_error(sut(jet_t{x, df}).df, expected), tolerance);
    }
}

INSTANTIATE_TEST_SUITE_P(
    core_math, model_curves_log_normal_math_test_t, ValuesIn(sweep_vectors), test_name_generator_t<vector_t>{});

//
// domain
//

TEST(model_curves_log_normal_domain_test_t, contains_lowest_finite_input)
{
    auto const eval = evaluator_t{params_t{2.0 / 3.0, 1.5, 1.0, 0.5}};
    EXPECT_TRUE(eval.domain().contains(std::numeric_limits<real_t>::lowest()));
}

TEST(model_curves_log_normal_domain_test_t, contains_largest_finite_input)
{
    auto const eval = evaluator_t{params_t{2.0 / 3.0, 1.5, 1.0, 0.5}};
    EXPECT_TRUE(eval.domain().contains(std::numeric_limits<real_t>::max()));
}

TEST(model_curves_log_normal_domain_test_t, lowest_finite_input_evaluates_finitely)
{
    auto const eval = evaluator_t{params_t{2.0 / 3.0, 1.5, 1.0, 0.5}};
    EXPECT_TRUE(std::isfinite(eval(std::numeric_limits<real_t>::lowest())));
}

TEST(model_curves_log_normal_domain_test_t, largest_finite_input_evaluates_finitely)
{
    auto const eval = evaluator_t{params_t{2.0 / 3.0, 1.5, 1.0, 0.5}};
    EXPECT_TRUE(std::isfinite(eval(std::numeric_limits<real_t>::max())));
}

//
// adapter
//

struct adapter_vector_t
{
    std::string_view name;
    real_t baseline;
    real_t limit;
    real_t acceleration_peak;
    real_t maximum_acceleration;

    friend auto operator<<(std::ostream& out, adapter_vector_t const& src) -> std::ostream&
    {
        return out << "{baseline = " << src.baseline << ", limit = " << src.limit
                   << ", acceleration_peak = " << src.acceleration_peak
                   << ", maximum_acceleration = " << src.maximum_acceleration << "}";
    }
};

adapter_vector_t const adapter_vectors[] = {
    {"standard", 0.0, 1.0, 5.0, 0.2},
    {"shifted_up", 2.0, 4.0, 5.0, 0.2},
    {"sharp", 0.5, 1.5, 2.0, 1.0},
    {"gradual", 0.0, 1.0, 10.0, 0.05},
};

struct model_curves_log_normal_adapter_test_t : TestWithParam<adapter_vector_t>
{
    real_t baseline = GetParam().baseline;
    real_t limit = GetParam().limit;
    real_t expected_peak = GetParam().acceleration_peak;
    real_t expected_max_accel = GetParam().maximum_acceleration;

    log_normal_t::config_t config{
        .baseline{"baseline", baseline},
        .limit{"limit", limit},
        .accel_peak{"peak", expected_peak},
        .max_accel{"max_accel", expected_max_accel},
    };

    // sigma comes out of a transcendental solve, so slope precision is solver-bound
    static constexpr auto slope_tolerance = 1e-5;

    // saturation is independent of (mu, sigma), so the endpoints hold to full precision regardless of the solver
    static constexpr auto value_tolerance = 1e-12;

    evaluator_t const sut{to_params<real_t>(config)};
};

TEST_P(model_curves_log_normal_adapter_test_t, adapter_yields_correct_inflection_slope)
{
    auto const y = sut(jet_t{expected_peak, df});

    EXPECT_LT(rel_error(y.df, df * expected_max_accel), slope_tolerance);
}

TEST_P(model_curves_log_normal_adapter_test_t, slope_peaks_at_configured_peak)
{
    auto const at_peak = sut(jet_t{expected_peak, df}).df;
    auto const below = sut(jet_t{0.9 * expected_peak, df}).df;
    auto const above = sut(jet_t{1.1 * expected_peak, df}).df;

    EXPECT_LT(below, at_peak);
    EXPECT_LT(above, at_peak);
}

TEST_P(model_curves_log_normal_adapter_test_t, baseline_passes_through_to_origin)
{
    EXPECT_EQ(baseline, sut(0.0));
}

TEST_P(model_curves_log_normal_adapter_test_t, limit_passes_through_to_asymptote)
{
    // z = (log(1e6) - sigma^2)/(sigma*sqrt2) >= 14 for every row: saturated far below value_tolerance
    EXPECT_NEAR(limit, sut(expected_peak * 1e6), value_tolerance);
}

INSTANTIATE_TEST_SUITE_P(adapter, model_curves_log_normal_adapter_test_t, ValuesIn(adapter_vectors),
    test_name_generator_t<adapter_vector_t>{});

} // namespace
} // namespace crv::model::curves
