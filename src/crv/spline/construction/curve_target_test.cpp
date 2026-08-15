// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "curve_target.hpp"
#include <crv/math/abs.hpp>
#include <crv/test/test.hpp>
#include <array>
#include <cmath>
#include <limits>

namespace crv::spline {
namespace {

using scalar_t = float_t;
using jet_t = jet_t<scalar_t>;

struct fractional_power_t
{
    scalar_t alpha;

    auto operator()(scalar_t x) const noexcept -> scalar_t { return std::pow(x, alpha); }

    auto operator()(jet_t x) const noexcept -> jet_t
    {
        auto const f = std::pow(x.f, alpha);
        auto const df = alpha * std::pow(x.f, alpha - 1.0) * x.df;
        return {f, df};
    }
};

TEST(curve_target_gain_test, exposes_gain_and_transfer_identity)
{
    auto const curve = fractional_power_t{.alpha = 0.5};
    auto const target = gain_curve_target_t{curve};

    for (auto const x : std::array{0.0, 0.25, 1.0, 9.0})
    {
        auto const expected_gain = curve(x);
        EXPECT_DOUBLE_EQ(expected_gain, target.gain(x));
        EXPECT_DOUBLE_EQ(x * expected_gain, target.transfer(x));
    }
}

TEST(curve_target_gain_test, singular_gain_derivative_at_origin_does_not_poison_transfer_jet)
{
    auto const curve = fractional_power_t{.alpha = 0.5};
    auto const target = gain_curve_target_t{curve};
    auto const input = jet_t{0.0, 3.0};

    auto const actual = target.transfer(input);

    EXPECT_DOUBLE_EQ(0.0, actual.f);
    EXPECT_DOUBLE_EQ(curve(0.0) * input.df, actual.df);
    EXPECT_FALSE(std::isnan(actual.df));
    EXPECT_TRUE(std::isfinite(actual.df));
}

TEST(curve_target_gain_test, transfer_jet_uses_product_rule_away_from_origin)
{
    auto const alpha = 0.5;
    auto const curve = fractional_power_t{.alpha = alpha};
    auto const target = gain_curve_target_t{curve};
    auto const input = jet_t{4.0, 2.5};

    auto const actual = target.transfer(input);
    auto const f = curve(input.f);
    auto const df = alpha * std::pow(input.f, alpha - 1.0);

    EXPECT_DOUBLE_EQ(input.f * f, actual.f);
    EXPECT_DOUBLE_EQ((f + input.f * df) * input.df, actual.df);
}

struct fake_antiderivative_t
{
    using scalar_t = float_t;

    constexpr auto mean_integrand(scalar_t x) const noexcept -> scalar_t { return 2.0 + x; }
    constexpr auto derivative(scalar_t x) const noexcept -> scalar_t { return 3.0 + 2.0 * x; }
};

TEST(curve_target_sensitivity_test, transfer_is_reconstructed_from_conditioned_gain_and_derivative_is_integrand)
{
    auto const target = sensitivity_curve_target_t{fake_antiderivative_t{}};
    auto const x = 4.0;
    auto const input = jet_t{x, 2.5};

    EXPECT_DOUBLE_EQ(2.0 + x, target.gain(x));
    EXPECT_DOUBLE_EQ(x * target.gain(x), target.transfer(x));

    auto const actual_jet = target.transfer(input);
    EXPECT_DOUBLE_EQ(x * target.gain(x), actual_jet.f);
    EXPECT_DOUBLE_EQ((3.0 + 2.0 * x) * input.df, actual_jet.df);
}

static_assert(gain_tolerance_to_integral_tolerance(scalar_t{256}, scalar_t{0x1p-40}) == scalar_t{0x1p-32});

TEST(curve_target_sensitivity_test, power_law_gain_and_transfer_remain_conditioned_below_spline_refinement_scale)
{
    auto const alpha = scalar_t{0.5};
    auto const curve = fractional_power_t{.alpha = alpha};
    auto constexpr domain_end = scalar_t{256};
    auto constexpr gain_tolerance = scalar_t{1e-9};
    auto constexpr depth_limit = int_t{64};

    auto const build_target = sensitivity_curve_target_builder_t<scalar_t>{
        .gain_tolerance = gain_tolerance,
        .depth_limit = depth_limit,
    };
    auto const result = build_target(curve, domain_end);
    auto const& target = result.target;

    EXPECT_FALSE(result.refinement_limited);

    auto const xs = std::array{
        scalar_t{0},
        std::ldexp(scalar_t{1}, -50),
        std::ldexp(scalar_t{1}, -40),
        std::ldexp(scalar_t{1}, -32),
        std::ldexp(scalar_t{1}, -24),
        std::ldexp(scalar_t{1}, -16),
        scalar_t{1e-3},
        scalar_t{0.25},
        scalar_t{1},
        scalar_t{16},
    };

    for (auto const x : xs)
    {
        auto const sensitivity = curve(x);
        auto const expected_gain = sensitivity / (alpha + 1.0);
        auto const expected_transfer = x * expected_gain;
        auto const actual_gain = target.gain(x);
        auto const actual_transfer = target.transfer(x);

        EXPECT_LE(abs(expected_gain - actual_gain), gain_tolerance)
            << "x=" << x << ", expected gain=" << expected_gain << ", actual gain=" << actual_gain;

        auto const transfer_rounding
            = scalar_t{8} * std::numeric_limits<scalar_t>::epsilon() * abs(expected_transfer);
        EXPECT_LE(abs(expected_transfer - actual_transfer), x * gain_tolerance + transfer_rounding)
            << "x=" << x << ", expected transfer=" << expected_transfer << ", actual transfer=" << actual_transfer;
        EXPECT_DOUBLE_EQ(x * actual_gain, actual_transfer);

        auto const input_tangent = scalar_t{1.75};
        auto const actual_jet = target.transfer(jet_t{x, input_tangent});
        EXPECT_DOUBLE_EQ(actual_transfer, actual_jet.f);
        EXPECT_DOUBLE_EQ(sensitivity * input_tangent, actual_jet.df);
    }
}

TEST(curve_target_sensitivity_test, builder_scales_gain_tolerance_into_integral_units)
{
    auto constexpr domain_end = scalar_t{64};
    auto constexpr gain_tolerance = scalar_t{0x1p-30};
    auto constexpr expected_integral_tolerance = scalar_t{0x1p-24};

    EXPECT_DOUBLE_EQ(
        expected_integral_tolerance, gain_tolerance_to_integral_tolerance(domain_end, gain_tolerance));
}

} // namespace
} // namespace crv::spline
