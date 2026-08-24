// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/spline/construction/curve_target.hpp>
#include <crv/spline/pipeline_config.hpp>
#include <crv/spline/spline_factory.hpp>
#include <crv/spline/spline_factory_policy.hpp>
#include <crv/test/test.hpp>
#include <algorithm>
#include <array>
#include <cmath>
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

    auto const find_origin = [&](int_t segment_index) noexcept -> x_t {
        if (segment_index == 0) return x_t{0};
        if (segment_index == segment_count) return locator.x_max();

        auto low = typename x_t::value_t{0};
        auto high = locator.x_max().value;
        while (low < high)
        {
            auto const midpoint = low + (high - low) / 2;
            if (locator.locate(x_t::literal(midpoint)).index < segment_index) low = midpoint + 1;
            else high = midpoint;
        }
        return x_t::literal(low);
    };

    for (auto segment_index = int_t{0}; segment_index < segment_count; ++segment_index)
    {
        auto const left = find_origin(segment_index);
        auto const right = find_origin(segment_index + 1);
        EXPECT_TRUE(spline.segments[segment_index].is_safe_through(right - left, left))
            << "segment=" << segment_index << " left_raw=" << left.value << " right_raw=" << right.value;
    }
}

template <typename target_t> auto build_spline(target_t const& target) -> spline_t
{
    auto spline = spline_t{};
    auto critical_points = std::vector<x_t>{to_fixed<x_t>(7.123456789), to_fixed<x_t>(31.0000003)};
    auto const result = spline_factory_t{}(spline, target, scalar_t{2e-6}, std::move(critical_points));
    EXPECT_TRUE(result);
    if (result) expect_all_segments_safe(spline);
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
    auto const built_target = sensitivity_curve_target_builder_t<scalar_t>{
        .gain_tolerance = scalar_t{1e-10},
        .depth_limit = 64,
    }(curve, scalar_t{policy_t::domain_end});
    ASSERT_FALSE(built_target.refinement_limited);

    auto const spline = build_spline(built_target.target);
    expect_gain_matches_target(spline, built_target.target, scalar_t{3e-6});

    // conditioned target has analytic mean gain x^alpha/(alpha+1)
    for (auto const x :
        std::array{std::ldexp(scalar_t{1}, -40), std::ldexp(scalar_t{1}, -24), scalar_t{1}, scalar_t{16}})
        EXPECT_NEAR(built_target.target.gain(x), std::pow(x, alpha) / (alpha + 1.0), 1e-10);
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
