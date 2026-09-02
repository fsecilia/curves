// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "curve_target.hpp"
#include <crv/math/abs.hpp>
#include <crv/quadrature/adaptive_integration_receipt.hpp>
#include <crv/test/test.hpp>
#include <array>
#include <cmath>
#include <gmock/gmock.h>
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

struct gain_curve_target_builder_test_t : Test
{
    gain_curve_target_builder_t sut;
};

TEST_F(gain_curve_target_builder_test_t, wraps_authored_gain_curve)
{
    auto const target = sut(fractional_power_t{.alpha = 0.5});
    EXPECT_DOUBLE_EQ(target.gain(4.0), 2.0);
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

struct sensitivity_curve_target_builder_test_t : Test
{
    using receipt_t = quadrature::adaptive_integration_receipt_t<scalar_t>;

    struct antiderivative_t
    {
        using scalar_t = crv::float_t;

        constexpr auto mean_integrand(scalar_t) const noexcept -> scalar_t { return scalar_t{2}; }
        constexpr auto derivative(scalar_t) const noexcept -> scalar_t { return scalar_t{3}; }
    };

    struct result_t
    {
        antiderivative_t antiderivative;
        receipt_t receipt;
    };

    struct mock_antiderivative_factory_t
    {
        virtual ~mock_antiderivative_factory_t() = default;
        MOCK_METHOD(result_t, call, (scalar_t domain_end, scalar_t tolerance, int_t depth_limit), (const));
    };
    StrictMock<mock_antiderivative_factory_t> mock_antiderivative_factory;

    struct antiderivative_factory_t
    {
        using scalar_t = crv::float_t;

        template <typename integrand_t>
        using antiderivative_t = sensitivity_curve_target_builder_test_t::antiderivative_t;

        mock_antiderivative_factory_t* mock = nullptr;

        template <typename curve_t>
        auto operator()(curve_t, scalar_t domain_end, scalar_t tolerance, int_t depth_limit) const -> result_t
        {
            return mock->call(domain_end, tolerance, depth_limit);
        }

        template <typename curve_t>
        auto operator()(curve_t, scalar_t domain_end, scalar_t tolerance, auto const&, int_t depth_limit) const
            -> result_t
        {
            return mock->call(domain_end, tolerance, depth_limit);
        }
    };

    using sut_t = sensitivity_curve_target_builder_t<antiderivative_factory_t>;

    static constexpr auto domain_end = scalar_t{64};
    static constexpr auto gain_tolerance = scalar_t{0x1p-30};
    static constexpr auto integral_tolerance = scalar_t{0x1p-24};
    static constexpr auto depth_limit = int_t{37};

    sut_t sut{
        .build_antiderivative = antiderivative_factory_t{&mock_antiderivative_factory},
        .gain_tolerance = gain_tolerance,
        .depth_limit = depth_limit,
    };
};

TEST_F(sensitivity_curve_target_builder_test_t, forwards_integral_tolerance_and_depth_limit)
{
    EXPECT_CALL(mock_antiderivative_factory, call(domain_end, integral_tolerance, depth_limit))
        .WillOnce(Return(result_t{.antiderivative = {},
            .receipt = {.requested_tolerance = integral_tolerance,
                .achieved_error = scalar_t{0x1p-27},
                .max_error = scalar_t{0x1p-28},
                .segment_count = 9,
                .refinement_limited = false}}));

    static_cast<void>(sut(fractional_power_t{.alpha = scalar_t{0.5}}, domain_end));
}

TEST_F(sensitivity_curve_target_builder_test_t, forwards_receipt_diagnostics)
{
    constexpr auto achieved_error = scalar_t{0x1p-27};
    EXPECT_CALL(mock_antiderivative_factory, call(domain_end, integral_tolerance, depth_limit))
        .WillOnce(Return(result_t{.antiderivative = {},
            .receipt = {.requested_tolerance = integral_tolerance,
                .achieved_error = achieved_error,
                .max_error = scalar_t{0x1p-28},
                .segment_count = 9,
                .refinement_limited = false}}));

    auto const result = sut(fractional_power_t{.alpha = scalar_t{0.5}}, domain_end);

    EXPECT_EQ(result.achieved_error, achieved_error);
}

TEST(curve_target_sensitivity_test, builder_scales_gain_tolerance_into_integral_units)
{
    auto constexpr domain_end = scalar_t{64};
    auto constexpr gain_tolerance = scalar_t{0x1p-30};
    auto constexpr expected_integral_tolerance = scalar_t{0x1p-24};

    EXPECT_DOUBLE_EQ(expected_integral_tolerance, gain_tolerance_to_integral_tolerance(domain_end, gain_tolerance));
}

} // namespace
} // namespace crv::spline
