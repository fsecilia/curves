// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "log_normal.hpp"
#include <crv/model/curves/test.hpp>
#include <crv/test/test.hpp>
#include <complex>
#include <vector>

namespace crv::model::curves {
namespace {

using real_t = float_t;

using evaluator_t = log_normal_t::evaluator_t<real_t>;
using complex_evaluator_t = log_normal_t::evaluator_t<std::complex<real_t>>;

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
    // center sweep (low to high), nominal width
    {"c0_5", 0.5, 0.5},
    {"c5", 5.0, 0.5},
    {"c50", 50.0, 0.5},
    {"c500", 500.0, 0.5},

    // width sweep (narrow to wide): controls sigma = sqrt(width) and hence c
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

// At x0 = exp(mu), the argument z = 0, so with c = 1/(sqrt(width)*sqrt2) and inv_sqrt_pi = 1/:
//
//     f(x0)    = 1/2
//     f'(x0)   =  (c / sqrt(pi)) / x0
//     f''(x0)  = -(c / sqrt(pi)) / x0^2
//     f'''(x0) = (2 * c * (1 - c^2) / sqrt(pi)) / x0^3
//
// This is exact across center and width. width is chosen so c != 1 in the f''' case (c = 1 makes 1 - c^2 = 0 and would
// mask a third-derivative scale/sign error).
struct model_curves_log_normal_median_test_t : TestWithParam<param_t>
{
    param_t const param = GetParam();
    evaluator_t const eval{make_config(param.center, param.width)};

    static constexpr real_t inv_sqrt_pi = std::numbers::inv_sqrtpi_v<real_t>;
    static constexpr auto tolerance = 1e-12;

    real_t const x0 = std::exp(std::log(param.center) + param.width);
    real_t const c = 1.0 / (std::sqrt(param.width) * std::sqrt(2.0));
};

TEST_P(model_curves_log_normal_median_test_t, value_is_half)
{
    EXPECT_NEAR(0.5, eval.derivatives<0>(x0).f, tolerance);
}

TEST_P(model_curves_log_normal_median_test_t, first_derivative_closed_form)
{
    EXPECT_LT(rel_error(eval.derivatives<1>(x0).d1, c * inv_sqrt_pi / x0), tolerance);
}

TEST_P(model_curves_log_normal_median_test_t, second_derivative_closed_form)
{
    EXPECT_LT(rel_error(eval.derivatives<2>(x0).d2, -c * inv_sqrt_pi / (x0 * x0)), tolerance);
}

TEST_P(model_curves_log_normal_median_test_t, third_derivative_closed_form)
{
    EXPECT_LT(rel_error(eval.derivatives<3>(x0).d3, 2.0 * c * (1.0 - c * c) * inv_sqrt_pi / (x0 * x0 * x0)), tolerance);
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

// below threshold_x_origin the curve has saturated to its floor: f = 0, and every derivative is zero. as x -> 0+ the
// gaussian e^{-z^2} (z -> -inf) drives f and all derivatives to 0, so the floor IS the limit; returning it also keeps
// log(x) away from -inf.
struct model_curves_log_normal_origin_test_t : Test
{
    evaluator_t const eval{make_config(5.0, 0.5)};

    static constexpr auto x = evaluator_t::threshold_x_origin / 1000.0;
    static constexpr auto tolerance = 1e-15;
};

TEST_F(model_curves_log_normal_origin_test_t, literal_zero_input)
{
    auto const d = eval.derivatives<3>(0.0);
    EXPECT_NEAR(0.0, d.f, tolerance);
    EXPECT_NEAR(0.0, d.d1, tolerance);
    EXPECT_NEAR(0.0, d.d2, tolerance);
    EXPECT_NEAR(0.0, d.d3, tolerance);
}

TEST_F(model_curves_log_normal_origin_test_t, value_is_floor)
{
    EXPECT_NEAR(0.0, eval.derivatives<0>(x).f, tolerance);
}

TEST_F(model_curves_log_normal_origin_test_t, all_derivatives_zero)
{
    auto const d = eval.derivatives<3>(x);
    EXPECT_NEAR(0.0, d.f, tolerance); // f
    EXPECT_NEAR(0.0, d.d1, tolerance); // f'
    EXPECT_NEAR(0.0, d.d2, tolerance); // f''
    EXPECT_NEAR(0.0, d.d3, tolerance); // f'''
}

//
// complex-step derivative
//

// This test sweeps the parameter space and a range of x across the median. The sweeps are based on the config
// constraints: center, width both in [1e-3, 1e3]. Each derivative order is differentiated by complex-step and compared
// to the next order down.
struct model_curves_log_normal_consistency_test_t : TestWithParam<param_t>
{
    param_t const param = GetParam();
    evaluator_t const eval{make_config(param.center, param.width)};
    complex_evaluator_t const ceval{make_config(param.center, param.width)};

    // multipliers of the median x0 = exp(mu), giving x well to either side. all stay clear of threshold_x_origin.
    static constexpr real_t multipliers[] = {0.02, 0.1, 0.3, 0.6, 0.9, 1.1, 2.0, 5.0, 50.0, 1e6};

    // complex-step is machine-precision in principle, but f''' carries a 1/x^3 conversion and three-term cancellation,
    // so allow headroom while still being tighter than approximation error.
    static constexpr auto tolerance = 1e-7;

    real_t x0() const { return std::exp(std::log(param.center) + param.width); }
};

TEST_P(model_curves_log_normal_consistency_test_t, first_derivative_matches_complex_step_of_value)
{
    for (auto const multipliers : multipliers)
    {
        auto const x = x0() * multipliers;
        auto const expected = complex_step_derivative([&](auto z) { return ceval.derivatives<0>(z).f; }, x);
        EXPECT_LT(rel_error(eval.derivatives<1>(x).d1, expected), tolerance);
    }
}

TEST_P(model_curves_log_normal_consistency_test_t, second_derivative_matches_complex_step_of_first)
{
    for (auto const multipliers : multipliers)
    {
        auto const x = x0() * multipliers;
        auto const expected = complex_step_derivative([&](auto z) { return ceval.derivatives<1>(z).d1; }, x);
        EXPECT_LT(rel_error(eval.derivatives<2>(x).d2, expected), tolerance);
    }
}

TEST_P(model_curves_log_normal_consistency_test_t, third_derivative_matches_complex_step_of_second)
{
    for (auto const multipliers : multipliers)
    {
        auto const x = x0() * multipliers;
        auto const expected = complex_step_derivative([&](auto z) { return ceval.derivatives<2>(z).d2; }, x);
        EXPECT_LT(rel_error(eval.derivatives<3>(x).d3, expected), tolerance);
    }
}

INSTANTIATE_TEST_SUITE_P(
    consistency, model_curves_log_normal_consistency_test_t, ValuesIn(sweep_params), test_name_generator_t<param_t>{});

//
// derivatives<n>()
//

struct model_curves_log_normal_derivatives_test_t : TestWithParam<param_t>
{
    param_t const param = GetParam();
    evaluator_t const eval{make_config(param.center, param.width)};

    static constexpr real_t multipliers[] = {0.1, 0.6, 0.9, 1.1, 2.0, 50.0, 1e6};
    static constexpr auto tolerance = 1e-15;

    auto x0() const noexcept -> real_t { return std::exp(std::log(param.center) + param.width); }
};

TEST_P(model_curves_log_normal_derivatives_test_t, truncation_does_not_perturb_lower_orders)
{
    for (auto const multipliers : multipliers)
    {
        auto const x = x0() * multipliers;

        auto const d0 = eval.derivatives<0>(x);
        auto const d1 = eval.derivatives<1>(x);
        auto const d2 = eval.derivatives<2>(x);
        auto const d3 = eval.derivatives<3>(x);

        // f identical at every requested order
        EXPECT_LT(rel_error(d0.f, d3.f), tolerance);
        EXPECT_LT(rel_error(d1.f, d3.f), tolerance);
        EXPECT_LT(rel_error(d2.f, d3.f), tolerance);

        // f' identical whether or not higher orders were also requested
        EXPECT_LT(rel_error(d1.d1, d3.d1), tolerance);
        EXPECT_LT(rel_error(d2.d1, d3.d1), tolerance);

        // f'' identical whether or not f''' was also requested
        EXPECT_LT(rel_error(d2.d2, d3.d2), tolerance);
    }
}

TEST_P(model_curves_log_normal_derivatives_test_t, operators_match_the_derivatives_surface)
{
    for (auto const multipliers : multipliers)
    {
        auto const x = x0() * multipliers;
        auto const d = eval.derivatives<3>(x);

        // scalar operator() is the value, i.e. derivatives<0>().f
        EXPECT_LT(rel_error(eval(x), d.f), tolerance);

        // jet operator() bridges derivatives<1>().d1 into jet_t.df (seed dx = 1 so the tangent passes through).
        auto const j = eval(typename evaluator_t::jet_t{x, 1.0});
        EXPECT_LT(rel_error(j.f, d.f), tolerance);
        EXPECT_LT(rel_error(j.df, d.d1), tolerance);
    }
}

INSTANTIATE_TEST_SUITE_P(
    orders, model_curves_log_normal_derivatives_test_t, ValuesIn(sweep_params), test_name_generator_t<param_t>{});

//
// critical points
//

// The log-normal CDF is strictly monotone: f' > 0 for every finite x, so there are no critical points. The api is
// honored and an empty vector is returned.
struct model_curves_log_normal_critical_points_test_t : Test
{
    evaluator_t const eval{make_config(5.0, 0.5)};
};

TEST_F(model_curves_log_normal_critical_points_test_t, none)
{
    EXPECT_TRUE(eval.critical_points().empty());
}

} // namespace
} // namespace crv::model::curves
