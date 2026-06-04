// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "log_normal.hpp"
#include <crv/signal_chain/curves/test.hpp>
#include <crv/test/test.hpp>
#include <complex>

namespace crv::model::curves {
namespace {

using real_t = float_t;

using evaluator_t = log_normal_t::evaluator_t<real_t>;
using complex_evaluator_t = log_normal_t::evaluator_t<std::complex<real_t>>;

real_t const df = 1.3;

constexpr auto make_config(real_t center, real_t width) noexcept -> log_normal_t::config_t
{
    auto config = log_normal_t::config_t{};
    config.center.value(center);
    config.width.value(width);
    return config;
}

// common param type for multiple tests
struct param_t
{
    std::string_view name;
    real_t center;
    real_t width;

    friend auto operator<<(std::ostream& out, param_t const& src) -> std::ostream&
    {
        return out << "{center = " << src.center << ", width = " << src.width << "}";
    }
};

// sweep table for multiple tests
param_t const sweep_params[] = {
    // center sweep, low to high, nominal width
    {"c0_5", 0.5, 0.5},
    {"c5", 5.0, 0.5},
    {"c50", 50.0, 0.5},
    {"c500", 500.0, 0.5},

    // width sweep, narrow to wide
    {"w0_01", 5.0, 0.01},
    {"w0_1", 5.0, 0.1},
    {"w0_5", 5.0, 0.5},
    {"w2", 5.0, 2.0},
    {"w10", 5.0, 10.0},
    {"w100", 5.0, 100.0},

    // combined extremes
    {"hi_all", 500.0, 100.0},
    {"lo_all", 0.5, 0.01},
};

//
// median point
//

// At x0 = exp(mu), the argument z = 0, so with c = 1/(sqrt(width)*sqrt2) and inv_sqrt_pi = 1/sqrt(pi):
//
//     f(x0)    = 1/2
//     f'(x0)   = c*inv_sqrt_pi/x0
//
struct model_curves_log_normal_median_test_t : TestWithParam<param_t>
{
    real_t center = GetParam().center;
    real_t width = GetParam().width;

    static constexpr real_t inv_sqrt_pi = std::numbers::inv_sqrtpi_v<real_t>;
    static constexpr auto tolerance = 1e-12;

    real_t const x0 = std::exp(std::log(center) + width);
    real_t const c = 1.0 / (std::sqrt(width) * std::sqrt(2.0));

    evaluator_t const sut{make_config(center, width)};
};

TEST_P(model_curves_log_normal_median_test_t, value_is_half)
{
    EXPECT_NEAR(0.5, sut(x0), tolerance);
}

TEST_P(model_curves_log_normal_median_test_t, first_derivative_closed_form)
{
    auto const y = sut(jet_t{x0, df});
    EXPECT_NEAR(0.5, y.f, tolerance);
    EXPECT_LT(rel_error(y.df, df * c * inv_sqrt_pi / x0), tolerance);
}

TEST_P(model_curves_log_normal_median_test_t, zero_tangent_propagates_zero_velocity)
{
    auto const y = sut(jet_t{x0, 0.0});
    EXPECT_NEAR(0.5, y.f, tolerance);
    EXPECT_EQ(0.0, y.df);
}

param_t const median_params[] = {
    {"c5_w0_25", 5.0, 0.25},
    {"c2_75_w0_5", 2.75, 0.5},
    {"c50_w0_1", 50.0, 0.1},
    {"c0_5_w2", 0.5, 2.0},
};
INSTANTIATE_TEST_SUITE_P(
    median, model_curves_log_normal_median_test_t, ValuesIn(median_params), test_name_generator_t<param_t>{});

//
// origin
//

// at the threshold and below the scalar and jet should always be saturated to literal zero in all fields
struct model_curves_log_normal_origin_test_t : Test
{
    evaluator_t const sut{make_config(5.0, 0.5)};
};

TEST_F(model_curves_log_normal_origin_test_t, literal_zero_scalar_input)
{
    auto const y = sut(0.0);
    EXPECT_EQ(0.0, y);
}

TEST_F(model_curves_log_normal_origin_test_t, literal_zero_jet_input)
{
    auto const y = sut(jet_t{0.0, 0.0});
    EXPECT_EQ(0.0, y.f);
    EXPECT_EQ(0.0, y.df);
}

TEST_F(model_curves_log_normal_origin_test_t, at_threshold_scalar_literal_zero)
{
    EXPECT_EQ(0.0, sut(evaluator_t::x_origin_saturation_threshold));
}

TEST_F(model_curves_log_normal_origin_test_t, at_threshold_jet_literal_zero)
{
    auto const result = sut(jet_t{evaluator_t::x_origin_saturation_threshold, df});
    EXPECT_EQ(0.0, result.f);
    EXPECT_EQ(0.0, result.df);
}

//
// asymptote
//

struct model_curves_log_normal_asymptote_test_t : Test
{
    static constexpr auto tolerance = 1e-12;
    static constexpr auto x_large = 1e6;

    evaluator_t const sut{make_config(5.0, 0.5)};
};

TEST_F(model_curves_log_normal_asymptote_test_t, scalar_saturates_at_high_input)
{
    EXPECT_NEAR(1.0, sut(x_large), tolerance);
}

TEST_F(model_curves_log_normal_asymptote_test_t, jet_saturates_at_high_input)
{
    auto const y = sut(jet_t{x_large, df});

    EXPECT_NEAR(1.0, y.f, tolerance);
    EXPECT_NEAR(0.0, y.df, tolerance);
}

//
// complex-step derivative
//

// This test sweeps the parameter space and a range of x across the median. The sweeps are based on the config
// constraints: center, width both in [1e-3, 1e3]. Each derivative order is differentiated by complex-step and compared
// to the next order down.
struct model_curves_log_normal_consistency_test_t : TestWithParam<param_t>
{
    real_t center = GetParam().center;
    real_t width = GetParam().width;

    // multipliers of the median x0 = exp(mu), giving x well to either side. all stay clear of threshold_x_origin.
    static constexpr real_t multipliers[] = {0.02, 0.1, 0.3, 0.6, 0.9, 1.1, 2.0, 5.0, 50.0, 1e6};

    static constexpr auto tolerance = 1e-12;

    real_t x0() const noexcept { return std::exp(std::log(center) + width); }

    evaluator_t const sut{make_config(center, width)};
    complex_evaluator_t const complex_sut{make_config(center, width)};
};

TEST_P(model_curves_log_normal_consistency_test_t, scalar_matches_jet_primal)
{
    for (auto const multipliers : multipliers)
    {
        auto const x = x0() * multipliers;
        EXPECT_LT(rel_error(sut(x), sut(jet_t{x, df}).f), tolerance);
    }
}

TEST_P(model_curves_log_normal_consistency_test_t, first_derivative_matches_complex_step_of_value)
{
    for (auto const multipliers : multipliers)
    {
        auto const x = x0() * multipliers;
        auto const expected = df * complex_step_derivative([&](auto z) { return complex_sut(z); }, x);
        EXPECT_LT(rel_error(sut(jet_t{x, df}).df, expected), tolerance);
    }
}

INSTANTIATE_TEST_SUITE_P(
    consistency, model_curves_log_normal_consistency_test_t, ValuesIn(sweep_params), test_name_generator_t<param_t>{});

//
// critical points
//

// The log-normal CDF is strictly monotone: f' > 0 for every finite x, so there are no critical points. The api is
// honored and an empty vector is returned.
struct model_curves_log_normal_critical_points_test_t : Test
{
    evaluator_t const sut{make_config(5.0, 0.5)};
};

TEST_F(model_curves_log_normal_critical_points_test_t, none)
{
    EXPECT_TRUE(sut.critical_points().empty());
}

} // namespace
} // namespace crv::model::curves
