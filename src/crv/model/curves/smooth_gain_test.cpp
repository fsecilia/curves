// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "smooth_gain.hpp"
#include <crv/model/curves/concepts.hpp>
#include <crv/test/test.hpp>
#include <cmath>
#include <limits>

namespace crv::model::curves {
namespace {

using real_t = float_t;
using params_t = smooth_gain_t::params_t<real_t>;
using evaluator_t = smooth_gain_t::evaluator_t<real_t>;

static_assert(is_curve<evaluator_t, real_t>);

struct smooth_gain_test_t : Test
{
    static constexpr auto v_0 = real_t{2};
    static constexpr auto v_50 = real_t{6};
    static constexpr auto k = real_t{2.27};
    static constexpr auto g_t = real_t{0.5};
    static constexpr auto g_f = real_t{8};
    static constexpr auto tolerance = real_t{1e-12};

    evaluator_t sut{params_t{.v_0 = v_0, .v_50 = v_50, .k = k, .g_t = g_t, .g_f = g_f}};
};

TEST_F(smooth_gain_test_t, returns_tracking_gain_before_transition)
{
    EXPECT_EQ(sut(real_t{1}), g_t);
}

TEST_F(smooth_gain_test_t, returns_tracking_gain_at_transition_start)
{
    EXPECT_EQ(sut(v_0), g_t);
}

TEST_F(smooth_gain_test_t, returns_geometric_mean_gain_at_half_transition)
{
    EXPECT_NEAR(sut(v_50), std::sqrt(g_t * g_f), tolerance);
}

TEST_F(smooth_gain_test_t, matches_naka_rushton_above_half_transition)
{
    auto const z = real_t{2};
    auto const z_k = std::pow(z, k);
    auto const transition = z_k / (real_t{1} + z_k);
    auto const expected = std::exp(std::log(g_t) + (std::log(g_f) - std::log(g_t)) * transition);
    auto const input = v_0 + z * (v_50 - v_0);

    EXPECT_NEAR(sut(input), expected, tolerance);
}

TEST_F(smooth_gain_test_t, remains_below_final_gain_after_half_transition)
{
    EXPECT_LT(sut(v_0 + real_t{10} * (v_50 - v_0)), g_f);
}

TEST_F(smooth_gain_test_t, propagates_derivative_at_half_transition)
{
    auto const tangent = real_t{1.3};
    auto const gain = std::sqrt(g_t * g_f);
    auto const transition_derivative = k / (real_t{4} * (v_50 - v_0));
    auto const expected = tangent * gain * (std::log(g_f) - std::log(g_t)) * transition_derivative;

    EXPECT_NEAR(sut(jet_t<real_t>{v_50, tangent}).df, expected, tolerance);
}

TEST_F(smooth_gain_test_t, derivative_is_zero_at_transition_start)
{
    EXPECT_EQ(sut(jet_t<real_t>{v_0, real_t{1}}).df, real_t{0});
}

TEST_F(smooth_gain_test_t, largest_finite_input_evaluates_to_finite_gain)
{
    EXPECT_TRUE(std::isfinite(sut(std::numeric_limits<real_t>::max())));
}

TEST_F(smooth_gain_test_t, input_domain_starts_at_zero)
{
    EXPECT_EQ(sut.input_domain().first(), real_t{0});
}

TEST_F(smooth_gain_test_t, input_domain_ends_at_largest_finite_value)
{
    EXPECT_EQ(sut.input_domain().last(), std::numeric_limits<real_t>::max());
}

TEST_F(smooth_gain_test_t, exposes_exact_lower_transition_boundary_as_critical_point)
{
    EXPECT_EQ(sut.critical_points(), (std::vector<real_t>{v_0}));
}

struct smooth_gain_adapter_test_t : Test
{
    smooth_gain_t::config_t config{
        .v_0{"v_0", -1.0},
        .v_50{"v_50", 12.0},
        .k{"k", 2.5},
        .g_t{"g_t", 0.75},
        .g_f{"g_f", 3.0},
    };
};

TEST_F(smooth_gain_adapter_test_t, config_converts_to_params)
{
    EXPECT_EQ(to_params<real_t>(config), (params_t{.v_0 = -1.0, .v_50 = 12.0, .k = 2.5, .g_t = 0.75, .g_f = 3.0}));
}

} // namespace
} // namespace crv::model::curves
