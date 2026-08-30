// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/math/abs.hpp>
#include <crv/quadrature/antiderivative_factory.hpp>
#include <crv/spline/construction/curve_target.hpp>
#include <crv/spline/pipeline_config.hpp>
#include <crv/spline/spline_factory.hpp>
#include <crv/spline/spline_factory_policy.hpp>
#include <crv/spline/validator.hpp>
#include <crv/test/test.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace crv::spline {
namespace {

using scalar_t = float_t;
using base_policy_t = default_spline_policy_t<scalar_t, prod_pipeline_config_t>;

// use a finer test-only refinement floor
//
// Production min_width is coarser than these near-zero probes. Tightening it here isolates gain-space AMR and fixed
// representation error from the production stopping policy.
struct policy_t : base_policy_t
{
    static constexpr auto log2_min_width = -44;
    using subdivision_predicate_t = crv::spline::subdivision_predicate_t<scalar_t, x_t, log2_min_width>;
    using refiner_t
        = crv::spline::refiner_t<typestates_t::unrefined_t, subdivider_t, subdivision_predicate_t, max_segment_count>;
    using spline_generator_t = crv::spline::spline_generator_t<scalar_t, x_t, spline_t, typestates_t, refinement_pool_t,
        refinement_pool_seeder_t, refiner_t, assembler_t>;
};
using x_t = policy_t::x_t;
using y_t = policy_t::y_t;
using spline_factory_t = spline_factory_t<policy_t, spline_generator_factory_t<policy_t>>;
using spline_t = spline_factory_t::spline_t;

auto evaluate_spline(spline_t const& spline, x_t x) noexcept -> y_t
{
    auto hint = spline_t::hint_t{};
    return spline.evaluate(x, hint);
}

struct fractional_power_t
{
    scalar_t alpha;

    auto operator()(scalar_t x) const noexcept -> scalar_t { return std::pow(x, alpha); }

    auto operator()(jet_t<scalar_t> x) const noexcept -> jet_t<scalar_t>
    {
        auto const f = std::pow(x.f, alpha);
        auto const df = alpha * std::pow(x.f, alpha - 1.0) * x.df;
        return {f, df};
    }
};

auto expect_all_segments_safe(spline_t const& spline) -> void
{
    auto const& locator = spline.segment_locator;
    auto const segment_count = locator.segment_count();
    ASSERT_GT(segment_count, 0);

    for (auto segment_index = int_t{0}; segment_index < segment_count; ++segment_index)
    {
        auto const left = locator.segment_origin(segment_index);
        auto const right = locator.segment_end(segment_index);
        EXPECT_TRUE(spline.segments[segment_index].is_safe_through(right - left, left))
            << "segment=" << segment_index << " left_raw=" << left.value << " right_raw=" << right.value;
    }
}

template <typename target_t> auto build_spline(target_t const& target) -> spline_t
{
    auto spline = spline_t{};
    auto critical_points = std::vector<x_t>{to_fixed<x_t>(7.123456789), to_fixed<x_t>(31.0000003)};
    auto const result = spline_factory_t{}(spline, target, policy_t::spline_gain_tolerance, std::move(critical_points));
    EXPECT_TRUE(result);
    if (result)
    {
        expect_all_segments_safe(spline);
        EXPECT_TRUE(spline_validator_t<spline_t>{}(spline));
    }
    return spline;
}

template <typename target_t>
auto expect_gain_matches_target(spline_t const& spline, target_t const& target, scalar_t tolerance) -> void
{
    auto const xs = std::array{
        x_t{0},
        x_t::literal(1),
        to_fixed<x_t>(std::ldexp(scalar_t{1}, -40)),
        to_fixed<x_t>(std::ldexp(scalar_t{1}, -32)),
        to_fixed<x_t>(std::ldexp(scalar_t{1}, -24)),
        to_fixed<x_t>(std::ldexp(scalar_t{1}, -16)),
        to_fixed<x_t>(scalar_t{1e-3}),
        to_fixed<x_t>(scalar_t{0.25}),
        x_t{1},
        x_t{16},
        x_t{255},
    };

    for (auto const x : xs)
    {
        auto const x_real = from_fixed<scalar_t>(x);
        auto const expected = target.gain(x_real);
        auto const actual = from_fixed<scalar_t>(evaluate_spline(spline, x));
        EXPECT_NEAR(actual, expected, tolerance * std::max(std::abs(expected), scalar_t{1})) << "x=" << x_real;
    }
}

TEST(spline_factory_integration_test, gain_authored_fractional_power_returns_gain_without_near_zero_transfer_division)
{
    auto const target = gain_curve_target_t{fractional_power_t{.alpha = 0.5}};
    auto const spline = build_spline(target);

    expect_gain_matches_target(spline, target, scalar_t{3e-6});
}

TEST(spline_factory_integration_test, sensitivity_authored_fractional_power_returns_conditioned_induced_gain)
{
    auto const alpha = scalar_t{0.5};
    auto const curve = fractional_power_t{.alpha = alpha};
    auto const built_target = sensitivity_curve_target_builder_t<quadrature::antiderivative_factory_t<scalar_t>>{
        .build_antiderivative = quadrature::antiderivative_factory_t<scalar_t>{},
        .gain_tolerance = policy_t::sensitivity_gain_tolerance,
        .depth_limit = policy_t::sensitivity_depth_limit,
    }(curve, scalar_t{policy_t::domain_end});
    ASSERT_FALSE(built_target.refinement_limited);

    auto const spline = build_spline(built_target.target);
    expect_gain_matches_target(spline, built_target.target, scalar_t{3e-6});

    // conditioned target has analytic mean gain x^alpha/(alpha+1)
    for (auto const x :
        std::array{std::ldexp(scalar_t{1}, -40), std::ldexp(scalar_t{1}, -24), scalar_t{1}, scalar_t{16}})
        EXPECT_NEAR(built_target.target.gain(x), std::pow(x, alpha) / (alpha + 1.0), 1e-10);
}

TEST(sensitivity_curve_target_integration_test, power_law_gain_and_transfer_remain_conditioned_below_spline_refinement_scale)
{
    auto const alpha = scalar_t{0.5};
    auto const curve = fractional_power_t{.alpha = alpha};
    auto constexpr domain_end = scalar_t{256};
    auto constexpr gain_tolerance = scalar_t{1e-9};
    auto constexpr depth_limit = int_t{64};

    auto const build_target =
        sensitivity_curve_target_builder_t<quadrature::antiderivative_factory_t<scalar_t>>{
            .build_antiderivative = quadrature::antiderivative_factory_t<scalar_t>{},
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

        auto const transfer_rounding = scalar_t{8} * std::numeric_limits<scalar_t>::epsilon() * abs(expected_transfer);
        EXPECT_LE(abs(expected_transfer - actual_transfer), x * gain_tolerance + transfer_rounding)
            << "x=" << x << ", expected transfer=" << expected_transfer << ", actual transfer=" << actual_transfer;
        EXPECT_DOUBLE_EQ(x * actual_gain, actual_transfer);

        auto const input_tangent = scalar_t{1.75};
        auto const actual_jet = target.transfer(jet_t<scalar_t>{x, input_tangent});
        EXPECT_DOUBLE_EQ(actual_transfer, actual_jet.f);
        EXPECT_DOUBLE_EQ(sensitivity * input_tangent, actual_jet.df);
    }
}

TEST(spline_factory_integration_test, knot_ownership_and_tail_are_continuous_in_gain_space)
{
    auto const target = gain_curve_target_t{fractional_power_t{.alpha = 0.5}};
    auto const spline = build_spline(target);
    auto const knot = to_fixed<x_t>(scalar_t{7.123456789});
    auto const before = x_t::literal(knot.value - 1);
    auto const after = x_t::literal(knot.value + 1);

    auto const before_location = spline.segment_locator.locate(before);
    auto const at_location = spline.segment_locator.locate(knot);
    auto const after_location = spline.segment_locator.locate(after);
    EXPECT_LT(before_location.origin, knot);
    EXPECT_EQ(at_location.origin, knot);
    EXPECT_EQ(after_location.origin, knot);

    // probe both sides of the knot
    //
    // The exact knot belongs to the right segment, where x == x0 returns its directly quantized g0.
    auto const left_before = spline.segments[before_location.index](before, before_location.origin);
    auto const left_at_knot = spline.segments[before_location.index](knot, before_location.origin);
    auto const right_at_knot = spline.segments[at_location.index](knot, at_location.origin);
    auto const right_after = spline.segments[after_location.index](after, after_location.origin);
    EXPECT_EQ(evaluate_spline(spline, before), left_before);
    EXPECT_EQ(evaluate_spline(spline, knot), right_at_knot);
    EXPECT_EQ(evaluate_spline(spline, after), right_after);

    auto const knot_gain = target.gain(from_fixed<scalar_t>(knot));
    EXPECT_NEAR(from_fixed<scalar_t>(left_at_knot), knot_gain, 3e-6);
    EXPECT_NEAR(from_fixed<scalar_t>(right_at_knot), knot_gain, 3e-6);

    auto const x_max = x_t{policy_t::domain_end};
    auto const inside = x_t::literal(x_max.value - 1);
    auto const final_location = spline.segment_locator.locate(inside);
    auto const fixed_endpoint = spline.segments[final_location.index](x_max, final_location.origin);

    EXPECT_EQ(spline.extend_final_tangent.y0, fixed_endpoint);
    EXPECT_EQ(evaluate_spline(spline, x_max), fixed_endpoint);

    auto const first_beyond = x_t::literal(x_max.value + 1);
    EXPECT_EQ(evaluate_spline(spline, first_beyond), spline.extend_final_tangent(x_t::literal(1)));
}

} // namespace
} // namespace crv::spline
