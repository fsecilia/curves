// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "power_law.hpp"
#include <crv/test/test.hpp>
#include <cmath>
#include <limits>

namespace crv::model::curves {
namespace {

using real_t = float_t;
using params_t = power_law_t::params_t<real_t>;
using evaluator_t = power_law_t::evaluator_t<real_t>;
using jet_t = crv::jet_t<real_t>;

struct model_curves_power_law_test_t : Test
{
    static constexpr auto p = real_t{2};
    static constexpr auto df = real_t{1.3};
};

TEST_F(model_curves_power_law_test_t, zero_power_is_exact_constant_at_origin)
{
    auto const sut = evaluator_t{params_t{p, 0.0}};
    EXPECT_EQ(sut(0.0), 1.0);
}

TEST_F(model_curves_power_law_test_t, zero_power_is_exact_constant_away_from_origin)
{
    auto const sut = evaluator_t{params_t{p, 0.0}};
    EXPECT_EQ(sut(256.0), 1.0);
}

TEST_F(model_curves_power_law_test_t, scalar_origin_is_zero_for_fractional_power)
{
    auto const sut = evaluator_t{params_t{p, 0.5}};
    EXPECT_EQ(sut(0.0), 0.0);
}

TEST_F(model_curves_power_law_test_t, unit_power_is_exact_linear_member)
{
    auto const sut = evaluator_t{params_t{p, 1.0}};
    EXPECT_EQ(sut(8.0), 4.0);
}

TEST_F(model_curves_power_law_test_t, pivot_has_unit_output)
{
    auto const sut = evaluator_t{params_t{p, 0.37}};
    EXPECT_EQ(sut(p), 1.0);
}

TEST_F(model_curves_power_law_test_t, integer_power_matches_known_value)
{
    auto const sut = evaluator_t{params_t{p, 2.0}};
    EXPECT_DOUBLE_EQ(sut(4.0), 4.0);
}

TEST_F(model_curves_power_law_test_t, fractional_power_matches_known_value)
{
    auto const sut = evaluator_t{params_t{p, 0.5}};
    EXPECT_DOUBLE_EQ(sut(8.0), 2.0);
}

TEST_F(model_curves_power_law_test_t, increases_on_positive_domain)
{
    auto const sut = evaluator_t{params_t{p, 0.5}};
    EXPECT_LT(sut(p / 2.0), sut(p * 2.0));
}

TEST_F(model_curves_power_law_test_t, jet_derivative_matches_closed_form_away_from_origin)
{
    auto const y = evaluator_t{params_t{p, 0.5}}(jet_t{8.0, df});
    EXPECT_DOUBLE_EQ(y.df, df * 0.125);
}

TEST_F(model_curves_power_law_test_t, zero_power_jet_is_exact_constant)
{
    auto const y = evaluator_t{params_t{p, 0.0}}(jet_t{8.0, df});
    EXPECT_EQ(y, (jet_t{1.0, 0.0}));
}

TEST_F(model_curves_power_law_test_t, unit_power_jet_is_exact_linear_member)
{
    auto const y = evaluator_t{params_t{p, 1.0}}(jet_t{8.0, df});
    EXPECT_EQ(y, (jet_t{4.0, df / p}));
}

TEST_F(model_curves_power_law_test_t, unit_power_has_finite_origin_derivative)
{
    auto const y = evaluator_t{params_t{p, 1.0}}(jet_t{0.0, df});
    EXPECT_DOUBLE_EQ(y.df, df / p);
}

TEST_F(model_curves_power_law_test_t, superlinear_power_has_zero_origin_derivative)
{
    auto const y = evaluator_t{params_t{p, 2.0}}(jet_t{0.0, df});
    EXPECT_EQ(y.df, 0.0);
}

TEST_F(model_curves_power_law_test_t, fractional_power_exposes_origin_derivative_singularity)
{
    auto const y = evaluator_t{params_t{p, 0.5}}(jet_t{0.0, df});
    EXPECT_TRUE(std::isinf(y.df));
}

TEST_F(model_curves_power_law_test_t, monotone_extension_begins_at_zero)
{
    auto const sut = evaluator_t{params_t{p, 0.5}};
    EXPECT_EQ(sut.monotone_extension_min(), 0.0);
}

TEST_F(model_curves_power_law_test_t, has_no_interior_critical_points)
{
    auto const sut = evaluator_t{params_t{p, 0.5}};
    EXPECT_TRUE(sut.critical_points().empty());
}

TEST_F(model_curves_power_law_test_t, finite_domain_check_accepts_zero_power_independent_of_p)
{
    auto const sut = evaluator_t{params_t{std::numeric_limits<real_t>::denorm_min(), 0.0}};
    EXPECT_TRUE(sut.finite_through(std::numeric_limits<real_t>::max()));
}

TEST_F(model_curves_power_law_test_t, finite_domain_check_accepts_representable_endpoint)
{
    auto const sut = evaluator_t{params_t{p, 0.5}};
    EXPECT_TRUE(sut.finite_through(256.0));
}

TEST_F(model_curves_power_law_test_t, finite_domain_check_rejects_overflowing_endpoint)
{
    auto const tiny_p = std::numeric_limits<real_t>::min();
    auto const sut = evaluator_t{params_t{tiny_p, 2.0}};
    EXPECT_FALSE(sut.finite_through(256.0));
}

TEST_F(model_curves_power_law_test_t, config_constraint_rejects_nonpositive_p)
{
    auto const config = power_law_t::config_t{};
    EXPECT_GT(config.unit_speed.constraint()(0.0), 0.0);
}

TEST_F(model_curves_power_law_test_t, config_constraint_accepts_zero_g)
{
    auto const config = power_law_t::config_t{};
    EXPECT_EQ(config.power.constraint()(0.0), 0.0);
}

TEST_F(model_curves_power_law_test_t, config_constraint_rejects_negative_g)
{
    auto const config = power_law_t::config_t{};
    EXPECT_EQ(config.power.constraint()(-1.0), 0.0);
}

TEST_F(model_curves_power_law_test_t, to_params_preserves_p_and_g)
{
    auto config = power_law_t::config_t{};
    config.unit_speed.value(3.5);
    config.power.value(0.25);

    EXPECT_EQ(to_params<real_t>(config), (params_t{3.5, 0.25}));
}

} // namespace
} // namespace crv::model::curves
