// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "smooth_gain.hpp"
#include <crv/model/curves/concepts.hpp>
#include <crv/model/curves/test.hpp>
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
    static constexpr auto g_t = real_t{2.0 / 3.0};
    static constexpr auto g_f = real_t{1.5};
    static constexpr auto v_50 = real_t{5};
    static constexpr auto elasticity = real_t{1};
    static constexpr auto tangent = real_t{1.3};
    static constexpr auto tolerance = real_t{1e-12};

    params_t const params{g_t, g_f, v_50, elasticity};
    evaluator_t const sut{params};
};

TEST_F(smooth_gain_test_t, returns_tracking_gain_at_origin)
{
    EXPECT_EQ(sut(real_t{0}), g_t);
}

TEST_F(smooth_gain_test_t, origin_derivative_is_zero)
{
    EXPECT_EQ(sut(jet_t<real_t>{real_t{0}, tangent}).df, real_t{0});
}

TEST_F(smooth_gain_test_t, reaches_geometric_gain_midpoint_at_v50)
{
    EXPECT_NEAR(sut(v_50), std::sqrt(g_t * g_f), tolerance);
}

TEST_F(smooth_gain_test_t, reaches_authored_peak_elasticity_at_v50)
{
    auto const y = sut(jet_t<real_t>{v_50, tangent});
    auto const actual = v_50 * y.df / (y.f * tangent);

    EXPECT_NEAR(actual, elasticity, tolerance);
}

TEST_F(smooth_gain_test_t, matches_naka_rushton_log_gain_form)
{
    auto const x = real_t{2} * v_50;
    auto const log_gain_delta = std::log(g_f / g_t);
    auto const k = real_t{4} * elasticity / log_gain_delta;
    auto const z_to_k = std::pow(x / v_50, k);
    auto const transition = z_to_k / (real_t{1} + z_to_k);
    auto const expected = g_t * std::exp(log_gain_delta * transition);

    EXPECT_NEAR(sut(x), expected, tolerance);
}

TEST_F(smooth_gain_test_t, zero_tangent_propagates_zero_derivative)
{
    EXPECT_EQ(sut(jet_t<real_t>{v_50, real_t{0}}).df, real_t{0});
}

TEST_F(smooth_gain_test_t, largest_finite_input_evaluates_finitely)
{
    EXPECT_TRUE(std::isfinite(sut(std::numeric_limits<real_t>::max())));
}

TEST_F(smooth_gain_test_t, smallest_positive_jet_evaluates_finitely)
{
    auto const x = std::nextafter(real_t{0}, real_t{1});
    auto const y = sut(jet_t<real_t>{x, tangent});

    EXPECT_TRUE(std::isfinite(y.f) && std::isfinite(y.df));
}

TEST_F(smooth_gain_test_t, input_domain_starts_at_zero)
{
    EXPECT_EQ(sut.input_domain().first(), real_t{0});
}

TEST_F(smooth_gain_test_t, input_domain_ends_at_largest_finite_value)
{
    EXPECT_EQ(sut.input_domain().last(), std::numeric_limits<real_t>::max());
}

TEST_F(smooth_gain_test_t, has_no_interior_critical_points)
{
    EXPECT_TRUE(sut.critical_points().empty());
}

TEST_F(smooth_gain_test_t, config_converts_to_params)
{
    auto const config = smooth_gain_t::config_t{
        .g_t{"g_t", 0.75},
        .g_f{"g_f", 3.0},
        .v_50{"v_50", 12.0},
        .elasticity{"elasticity", 1.25},
    };

    EXPECT_EQ(to_params<real_t>(config), (params_t{0.75, 3.0, 12.0, 1.25}));
}

} // namespace
} // namespace crv::model::curves
