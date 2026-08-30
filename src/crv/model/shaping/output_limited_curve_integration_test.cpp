// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/model/curves/power_law.hpp>
#include <crv/model/shaping/output_limited_curve.hpp>
#include <crv/model/shaping/transforms/compact_output_limiter.hpp>
#include <crv/model/shaping/transitions/smoothstep.hpp>
#include <crv/test/test.hpp>
#include <cmath>

namespace crv::shaping {
namespace {

struct shaping_output_limited_curve_power_law_integration_test_t : Test
{
    using scalar_t = float_t;
    using jet_t = crv::jet_t<scalar_t>;
    using transition_t = transitions::smoothstep_t;
    using upper_t = transforms::upper_output_limiter_t<scalar_t, transition_t>;
    using lower_t = transforms::lower_output_limiter_t<scalar_t, transition_t>;
    using power_law_t = model::curves::power_law_t::evaluator_t<scalar_t>;
    using params_t = model::curves::power_law_t::params_t<scalar_t>;

    static constexpr auto smoothstep_half_integral = scalar_t{3.0 / 32.0};

    static auto upper_half_width(scalar_t bound, scalar_t delta_y) -> scalar_t
    {
        return -std::log1p(-delta_y / bound) / (scalar_t{2} * smoothstep_half_integral);
    }

    static auto lower_half_width(scalar_t bound, scalar_t delta_y) -> scalar_t
    {
        return std::log1p(delta_y / bound) / (scalar_t{2} * smoothstep_half_integral);
    }

    static auto upper_lower_log(scalar_t bound, scalar_t delta_y) -> scalar_t
    {
        return std::log(bound) - upper_half_width(bound, delta_y);
    }

    static auto upper_upper_log(scalar_t bound, scalar_t delta_y) -> scalar_t
    {
        return std::log(bound) + upper_half_width(bound, delta_y);
    }

    static auto lower_upper_log(scalar_t bound, scalar_t delta_y) -> scalar_t
    {
        return std::log(bound) + lower_half_width(bound, delta_y);
    }

    static auto upper_delta_for_half_width(scalar_t bound, scalar_t half_width) -> scalar_t
    {
        return bound * (scalar_t{1} - std::exp(-scalar_t{2} * half_width * smoothstep_half_integral));
    }

    static auto lower_delta_for_half_width(scalar_t bound, scalar_t half_width) -> scalar_t
    {
        return bound * (std::exp(scalar_t{2} * half_width * smoothstep_half_integral) - scalar_t{1});
    }

    static auto make_power_law(scalar_t power = scalar_t{1}, scalar_t unit_speed = scalar_t{1}) -> power_law_t
    {
        return power_law_t{params_t{.unit_speed = unit_speed, .power = power}};
    }

    static auto make_upper(scalar_t bound, scalar_t delta_y) -> upper_t
    {
        return upper_t::make(bound, delta_y, transition_t{}).value();
    }

    static auto make_lower(scalar_t bound, scalar_t delta_y) -> lower_t
    {
        return lower_t::make(bound, delta_y, transition_t{}).value();
    }

    static auto make_sequential(scalar_t lower_bound, scalar_t lower_delta, scalar_t upper_bound, scalar_t upper_delta)
    {
        auto lower_curve = output_limited_curve_t{make_lower(lower_bound, lower_delta), make_power_law()};
        return output_limited_curve_t{make_upper(upper_bound, upper_delta), std::move(lower_curve)};
    }
};

TEST_F(
    shaping_output_limited_curve_power_law_integration_test_t, fractional_power_law_origin_is_exact_lower_plateau_jet)
{
    auto const limiter = make_lower(0.25, 0.1);
    auto const sut = output_limited_curve_t{limiter, make_power_law(0.5, 2.0)};
    EXPECT_EQ(sut(jet_t{0.0, 1.0}), (jet_t{0.25, 0.0}));
}

TEST_F(
    shaping_output_limited_curve_power_law_integration_test_t, fractional_power_law_jet_resumes_inside_lower_transition)
{
    auto const bound = scalar_t{0.25};
    auto const limiter = make_lower(bound, 0.1);
    auto const unit_speed = scalar_t{2};
    auto const power = scalar_t{0.5};
    auto const input = unit_speed * std::pow(bound, scalar_t{1} / power);
    auto const sut = output_limited_curve_t{limiter, make_power_law(power, unit_speed)};
    EXPECT_TRUE(std::isfinite(sut(jet_t{input, 1.0}).df));
}

TEST_F(shaping_output_limited_curve_power_law_integration_test_t,
    upper_limiter_reaches_exact_ceiling_over_growing_power_law)
{
    auto const bound = scalar_t{4};
    auto const delta_y = scalar_t{0.5};
    auto const limiter = make_upper(bound, delta_y);
    auto const curve_output = std::exp(upper_upper_log(bound, delta_y) + scalar_t{1});
    auto const unit_speed = scalar_t{2};
    auto const power = scalar_t{0.5};
    auto const input = unit_speed * std::pow(curve_output, scalar_t{1} / power);
    auto const sut = output_limited_curve_t{limiter, make_power_law(power, unit_speed)};
    EXPECT_EQ(sut(input), bound);
}

TEST_F(shaping_output_limited_curve_power_law_integration_test_t, zero_upper_can_skip_power_law_domain_entirely)
{
    auto const limiter = upper_t::make(0.0, 0.0, transition_t{}).value();
    auto const sut = output_limited_curve_t{limiter, make_power_law(0.5, 2.0)};
    EXPECT_EQ(sut(-1.0), 0.0);
}

TEST_F(shaping_output_limited_curve_power_law_integration_test_t,
    well_separated_bounds_leave_middle_output_exactly_unchanged)
{
    auto const lower_bound = scalar_t{1};
    auto const lower_delta = scalar_t{0.1};
    auto const upper_bound = scalar_t{4};
    auto const upper_delta = scalar_t{0.4};
    auto const lower = make_lower(lower_bound, lower_delta);
    auto const upper = make_upper(upper_bound, upper_delta);
    auto const middle_log
        = (lower_upper_log(lower_bound, lower_delta) + upper_lower_log(upper_bound, upper_delta)) / scalar_t{2};
    auto const input = std::exp(middle_log);
    auto const lower_curve = output_limited_curve_t{lower, make_power_law()};
    auto const sut = output_limited_curve_t{upper, lower_curve};
    EXPECT_EQ(sut(input), input);
}

TEST_F(shaping_output_limited_curve_power_law_integration_test_t, nearby_supports_preserve_monotonic_order)
{
    auto const half_width = scalar_t{0.2};
    auto const lower_bound = scalar_t{1};
    auto const upper_bound = std::exp(scalar_t{2} * half_width);
    auto const contact = std::exp(half_width);
    auto const sut = make_sequential(lower_bound, lower_delta_for_half_width(lower_bound, half_width), upper_bound,
        upper_delta_for_half_width(upper_bound, half_width));
    EXPECT_LE(sut(contact * 0.99), sut(contact * 1.01));
}

TEST_F(shaping_output_limited_curve_power_law_integration_test_t, overlapping_supports_preserve_monotonic_order)
{
    auto const sut = make_sequential(1.2, 0.3, 1.4, 0.3);
    EXPECT_LE(sut(0.5), sut(2.0));
}

TEST_F(shaping_output_limited_curve_power_law_integration_test_t,
    later_upper_limiter_wins_when_lower_nominal_bound_is_higher)
{
    auto const sut = make_sequential(2.0, 0.2, 1.5, 0.01);
    EXPECT_EQ(sut(0.0), 1.5);
}

TEST_F(shaping_output_limited_curve_power_law_integration_test_t,
    final_upper_ceiling_remains_exact_with_overlapping_supports)
{
    auto const sut = make_sequential(1.2, 0.3, 1.4, 0.3);
    EXPECT_EQ(sut(100.0), 1.4);
}

} // namespace
} // namespace crv::shaping
