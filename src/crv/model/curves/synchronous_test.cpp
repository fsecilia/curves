// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "synchronous.hpp"
#include <crv/model/curves/test.hpp>
#include <crv/test/test.hpp>
#include <complex>
#include <vector>

namespace crv::model::curves {
namespace {

using real_t = float_t;

using evaluator_t = synchronous_t::evaluator_t<real_t>;
using complex_evaluator_t = synchronous_t::evaluator_t<std::complex<real_t>>;

constexpr auto make_config(real_t motivity, real_t gamma, real_t smooth, real_t sync_speed) noexcept
    -> synchronous_t::config_t
{
    auto config = synchronous_t::config_t{};
    config.motivity.value(motivity);
    config.gamma.value(gamma);
    config.smooth.value(smooth);
    config.sync_speed.value(sync_speed);
    return config;
}

// common param type for multiple tests
struct param_t
{
    std::string_view name;
    real_t motivity;
    real_t gamma;
    real_t smooth;
    real_t sync_speed;

    friend auto operator<<(std::ostream& out, param_t const& src) -> std::ostream&
    {
        return out << "{motivity = " << src.motivity << ", gamma = " << src.gamma << ", smooth = " << src.smooth
                   << ", sync_speed = " << src.sync_speed << "}";
    }
};

// sweep table for multiple tests
param_t const sweep_params[] = {
    // motivity sweep
    {"m1_5", 1.5, 2.0, 0.5, 5.0},
    {"m10", 10.0, 2.0, 0.5, 5.0},
    {"m100", 100.0, 2.0, 0.5, 5.0},
    {"m1000", 1000.0, 2.0, 0.5, 5.0},

    // gamma sweep
    {"g0_5", 10.0, 0.5, 0.5, 5.0},
    {"g1", 10.0, 1.0, 0.5, 5.0},
    {"g3", 10.0, 3.0, 0.5, 5.0},
    {"g10", 10.0, 10.0, 0.5, 5.0},

    // smooth sweep
    {"k1", 10.0, 2.0, 1.0, 5.0},
    {"k0_5", 10.0, 2.0, 0.5, 5.0},
    {"k0_25", 10.0, 2.0, 0.25, 5.0},
    {"k0_125", 10.0, 2.0, 0.125, 5.0},
    {"k0_0625", 10.0, 2.0, 0.0625, 5.0}, // k = 8, the floor

    // sync speed sweep
    {"p0_5", 10.0, 2.0, 0.5, 0.5},
    {"p50", 10.0, 2.0, 0.5, 50.0},

    // combined extremes
    {"hi_all", 1000.0, 10.0, 0.0625, 50.0},
    {"lo_all", 1.5, 0.5, 1.0, 0.5},
};

//
// cusp
//

// At the cusp the curve is exactly the power law (x/p)^g, so at x = p:
//    f(p)    = 1
//    f'(p)   = g/p
//    f''(p)  = g(g-1)/p^2
//    f'''(p) = g(g-1)(g-2)/p^3
struct model_curves_synchronous_cusp_test_t : TestWithParam<param_t>
{
    real_t motivity = GetParam().motivity;
    real_t gamma = GetParam().gamma;
    real_t smooth = GetParam().smooth;
    real_t sync_speed = GetParam().sync_speed;

    evaluator_t const eval{make_config(motivity, gamma, smooth, sync_speed)};

    real_t const p = sync_speed;
    real_t const g = gamma;

    static constexpr auto tolerance = 1e-12;
};

TEST_P(model_curves_synchronous_cusp_test_t, value_is_one)
{
    EXPECT_NEAR(1.0, eval.derivatives<0>(p).f, tolerance);
}

TEST_P(model_curves_synchronous_cusp_test_t, first_derivative)
{
    EXPECT_LT(rel_error(eval.derivatives<1>(p).d1, g / p), tolerance);
}

TEST_P(model_curves_synchronous_cusp_test_t, second_derivative)
{
    EXPECT_LT(rel_error(eval.derivatives<2>(p).d2, g * (g - 1.0) / (p * p)), tolerance);
}

TEST_P(model_curves_synchronous_cusp_test_t, third_derivative)
{
    EXPECT_LT(rel_error(eval.derivatives<3>(p).d3, g * (g - 1.0) * (g - 2.0) / (p * p * p)), tolerance);
}

param_t const cusp_params[] = {
    {"g3_p5", 2.0, 3.0, 0.5, 5.0},
    {"g2_5_p2_75", 100.0, 2.5, 0.5, 2.75},
    {"g10_p50", 1.5, 10.0, 0.5, 50.0},
    {"g2_p0_5", 1000.0, 2.0, 0.5, 0.5},
};
INSTANTIATE_TEST_SUITE_P(
    cusp, model_curves_synchronous_cusp_test_t, ValuesIn(cusp_params), test_name_generator_t<param_t>{});

//
// origin
//

// Below threshold_x_origin the curve has saturated to its floor: f = 1/m, and every derivative is zero.
struct model_curves_synchronous_origin_test_t : Test
{
    evaluator_t const eval{make_config(2.0, 3.0, 0.5, 5.0)};

    static constexpr auto x = 1e-15;
    static constexpr auto tolerance = 1e-15;
};

TEST_F(model_curves_synchronous_origin_test_t, literal_zero_input)
{
    // returns saturated limit 1/m, not NaN
    auto const d = eval.derivatives<3>(0.0);
    EXPECT_NEAR(1.0 / 2.0, d.f, tolerance);
    EXPECT_NEAR(0.0, d.d1, tolerance);
    EXPECT_NEAR(0.0, d.d2, tolerance);
    EXPECT_NEAR(0.0, d.d3, tolerance);
}

TEST_F(model_curves_synchronous_origin_test_t, value_is_floor)
{
    EXPECT_NEAR(1.0 / 2.0, eval.derivatives<0>(x).f, tolerance);
}

TEST_F(model_curves_synchronous_origin_test_t, all_derivatives_zero)
{
    auto const d = eval.derivatives<3>(x);
    EXPECT_NEAR(1.0 / 2.0, d.f, tolerance); // f
    EXPECT_NEAR(0.0, d.d1, tolerance); // f'
    EXPECT_NEAR(0.0, d.d2, tolerance); // f''
    EXPECT_NEAR(0.0, d.d3, tolerance); // f'''
}

//
// complex-step derivative
//

// This test sweeps the parameter space and a range of x on both sides of the cusp. The nonholomorphic neighborhood of
// the cusp is skipped. The sweeps are based on the config constraints: m in [1,1e3], g in [1e-3,1e3], smooth in
// [1/16,1], p in [1e-3,1e3]. Each derivative order is differentiated by complex-step and compared to the next order
// down.
struct model_curves_synchronous_consistency_test_t : TestWithParam<param_t>
{
    real_t motivity = GetParam().motivity;
    real_t gamma = GetParam().gamma;
    real_t smooth = GetParam().smooth;
    real_t sync_speed = GetParam().sync_speed;

    real_t const p = sync_speed;

    evaluator_t const eval{make_config(motivity, gamma, smooth, sync_speed)};
    complex_evaluator_t const ceval{make_config(motivity, gamma, smooth, sync_speed)};

    // multipliers of p giving x on both sides of the cusp
    //
    // The closest are still outside the |u| <= threshold_u window for the swept parameters, and the loop guards the
    // window explicitly.
    static constexpr real_t multipliers[] = {0.05, 0.2, 0.5, 0.7, 0.95, 1.05, 1.5, 3.0, 30.0, 1e6};

    // complex-step is machine-precision in principle, but f''' carries a 1/x^3 conversion and three-term cancellation,
    // so allow headroom while still being tighter than approximation error.
    static constexpr auto tolerance = 1e-7;

    // skip x within this log-distance of the cusp; that region has a dedicated test
    static constexpr auto cusp_skip = 1e-2;

    // returns true when definitely outside the neigorhood of the cusp
    auto on_smooth_branch(real_t x) const noexcept -> bool
    {
        using std::log;
        return abs(log(x) - log(p)) > cusp_skip;
    }
};

TEST_P(model_curves_synchronous_consistency_test_t, first_derivative_matches_complex_step_of_value)
{
    for (auto const multiplier : multipliers)
    {
        auto const x = p * multiplier;
        if (!on_smooth_branch(x)) continue;
        auto const expected = complex_step_derivative([&](auto z) { return ceval.derivatives<0>(z).f; }, x);
        EXPECT_LT(rel_error(eval.derivatives<1>(x).d1, expected), tolerance);
    }
}

TEST_P(model_curves_synchronous_consistency_test_t, second_derivative_matches_complex_step_of_first)
{
    for (auto const multiplier : multipliers)
    {
        auto const x = p * multiplier;
        if (!on_smooth_branch(x)) continue;
        auto const expected = complex_step_derivative([&](auto z) { return ceval.derivatives<1>(z).d1; }, x);
        EXPECT_LT(rel_error(eval.derivatives<2>(x).d2, expected), tolerance);
    }
}

TEST_P(model_curves_synchronous_consistency_test_t, third_derivative_matches_complex_step_of_second)
{
    for (auto const multiplier : multipliers)
    {
        auto const x = p * multiplier;
        if (!on_smooth_branch(x)) continue;
        auto const expected = complex_step_derivative([&](auto z) { return ceval.derivatives<2>(z).d2; }, x);
        EXPECT_LT(rel_error(eval.derivatives<3>(x).d3, expected), tolerance);
    }
}

INSTANTIATE_TEST_SUITE_P(
    consistency, model_curves_synchronous_consistency_test_t, ValuesIn(sweep_params), test_name_generator_t<param_t>{});

//
// derivatives<n>()
//

struct model_curves_synchronous_derivatives_test_t : TestWithParam<param_t>
{
    real_t motivity = GetParam().motivity;
    real_t gamma = GetParam().gamma;
    real_t smooth = GetParam().smooth;
    real_t sync_speed = GetParam().sync_speed;

    real_t const p = sync_speed;

    evaluator_t const eval{make_config(motivity, gamma, smooth, sync_speed)};

    static constexpr real_t multipliers[] = {0.2, 0.7, 0.95, 1.05, 1.5, 30.0};
    static constexpr auto tolerance = 1e-15; // exact: same arithmetic, just truncated -- expect bit-identical
};

TEST_P(model_curves_synchronous_derivatives_test_t, truncation_does_not_perturb_lower_orders)
{
    for (auto const multiplier : multipliers)
    {
        auto const x = p * multiplier;

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

TEST_P(model_curves_synchronous_derivatives_test_t, operators_match_the_derivatives_surface)
{
    for (auto const multiplier : multipliers)
    {
        auto const x = p * multiplier;
        auto const d = eval.derivatives<3>(x);

        // scalar operator() is the value, i.e. derivatives<0>().f
        EXPECT_LT(rel_error(eval(x), d.f), tolerance);

        // jet operator() bridges derivatives<1>().d1 into jet_t.df
        auto const j = eval(typename evaluator_t::jet_t{x, 1.0});
        EXPECT_LT(rel_error(j.f, d.f), tolerance);
        EXPECT_LT(rel_error(j.df, d.d1), tolerance);
    }
}

INSTANTIATE_TEST_SUITE_P(
    orders, model_curves_synchronous_derivatives_test_t, ValuesIn(sweep_params), test_name_generator_t<param_t>{});

//
// cusp-branch seam
//

// This tests that the power-law region is as narrow as precision allows. The cusp branch fires on |u| <= threshold_u
// and returns the power law (x/p)^g instead of the tanh form. If that threshold is too wide, the power law replaces
// real tanh curvature, producing a seam where the branches meet. The discontinuity shows up under zoom and stalls the
// inverse bisection. These tests guard that. At a correct threshold the two branches agree to ~eps at the seam, so
// there is no kink.
struct model_curves_synchronous_seam_test_t : TestWithParam<param_t>
{
    real_t motivity = GetParam().motivity;
    real_t gamma = GetParam().gamma;
    real_t smooth = GetParam().smooth;
    real_t sync_speed = GetParam().sync_speed;

    real_t const p = sync_speed;

    evaluator_t const eval{make_config(motivity, gamma, smooth, sync_speed)};
};

// black-box
//
// This tests that there is no curvature spike at the cusp. Sample f and f' on a tight grid straddling x=p and check the
// discrete second difference stays bounded. A branch seam shows up as a localized jump.
TEST_P(model_curves_synchronous_seam_test_t, no_kink_across_the_cusp)
{
    // step in log-x; samples are uniform in u
    auto const h = 1e-6;

    // sample f', the order-1 tangent, across the seam; f' is where a value-level power-law seam first becomes visible
    auto const sample_d1 = [&](real_t k) { return eval.derivatives<1>(p * std::exp(k * h)).d1; };

    // discrete third difference of f' over consecutive steps
    //
    // For a smooth function this is O(h) and tiny. A seam makes one of these blow up relative to its neighbors.
    auto prev_second_diff = sample_d1(-1) - 2.0 * sample_d1(0) + sample_d1(1);
    for (int k = -20; k <= 20; ++k)
    {
        auto const second_diff = sample_d1(k - 1) - 2.0 * sample_d1(k) + sample_d1(k + 1);

        // Neighboring second differences of a smooth sampling vary slowly. Allow a generous factor before flagging.
        // A real seam produces a spike orders of magnitude above this.
        auto const scale = std::max({std::abs(second_diff), std::abs(prev_second_diff), 1e-12});
        EXPECT_LT(std::abs(second_diff - prev_second_diff) / scale, 10.0);
        prev_second_diff = second_diff;
    }
}

// white-box
//
// At the branch boundary the smooth form must equal the power law to ~eps. Reconstruct the seam location from the same
// closed form threshold_u uses, sample the evaluator just outside the cusp window, and confirm f there matches (x/p)^g.
TEST_P(model_curves_synchronous_seam_test_t, smooth_branch_matches_power_law_at_seam)
{
    using std::exp;
    using std::log;
    using std::pow;

    auto const m = static_cast<real_t>(motivity);
    auto const g = static_cast<real_t>(gamma);
    auto const k = 0.5 / static_cast<real_t>(smooth);
    auto const L = log(m);
    auto const G = g / L;
    auto const r = 1.0 / k;
    auto const eps = std::numeric_limits<real_t>::epsilon();

    auto const threshold_u = pow(3.0 * eps / (L * r), 1.0 / (1.0 + 2.0 * k));

    // step just outside the window on the smooth branch, then compare to the power law.
    auto const u = 1.01 * threshold_u;
    auto const x = p * exp(u / G); // u = G*(log x - log p) > 0

    auto const f_eval = eval.derivatives<0>(x).f; // smooth branch
    auto const f_power = pow(x / p, g);

    // divergence is still ~eps-scale just outside the eps-window
    EXPECT_LT(rel_error(f_eval, f_power), 1e-10);
}

INSTANTIATE_TEST_SUITE_P(
    seam, model_curves_synchronous_seam_test_t, ValuesIn(sweep_params), test_name_generator_t<param_t>{});

//
// critical points
//

// The curve emits exactly one critical point, the cusp at the sync speed, in its own input coordinate, raw and
// unquantized.
struct model_curves_synchronous_critical_points_test_t : Test
{
    static constexpr auto sync_speed = 5.0;
    evaluator_t const eval{make_config(2.0, 3.0, 0.5, sync_speed)};
};

TEST_F(model_curves_synchronous_critical_points_test_t, single_point_at_sync_speed)
{
    auto const points = eval.critical_points();
    ASSERT_EQ(1u, points.size());
    EXPECT_NEAR(sync_speed, points.front(), 1e-15);
}

} // namespace
} // namespace crv::model::curves
