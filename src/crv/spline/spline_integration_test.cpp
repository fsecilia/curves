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

// The production minimum width is intentionally much coarser than the near-zero probes below. Tighten only the test
// refinement floor so this integration test measures gain-space AMR and fixed representation accuracy rather than the
// production policy's stopping rule.
struct policy_t : base_policy_t
{
    static constexpr auto log2_min_width = -44;
    using subdivision_predicate_t = crv::spline::subdivision_predicate_t<scalar_t, x_t, log2_min_width>;
    using refiner_t = crv::spline::refiner_t<typename typestates_t::unrefined_t, subdivider_t, subdivision_predicate_t,
        max_segment_count>;
    using spline_generator_t = crv::spline::spline_generator_t<scalar_t, x_t, spline_t, typestates_t, refinement_pool_t,
        refinement_pool_seeder_t, refiner_t, assembler_t>;
};
using x_t = policy_t::x_t;
using y_t = policy_t::y_t;
using spline_factory_t = spline_factory_t<policy_t, spline_generator_factory_t<policy_t>>;
using spline_t = spline_factory_t::spline_t;

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

template <typename target_t> auto build_spline(target_t const& target) -> spline_t
{
    auto spline = spline_t{};
    auto critical_points = std::vector<x_t>{to_fixed<x_t>(7.123456789), to_fixed<x_t>(31.0000003)};
    spline_factory_t{}(spline, target, scalar_t{2e-6}, std::move(critical_points));
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
        auto const actual = from_fixed<scalar_t>(spline(x));
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

    // The conditioned target itself has the analytic mean gain x^alpha/(alpha+1).
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

    auto const before_location = spline.payload.segment_locator.locate(before);
    auto const at_location = spline.payload.segment_locator.locate(knot);
    auto const after_location = spline.payload.segment_locator.locate(after);
    EXPECT_LT(before_location.origin, knot);
    EXPECT_EQ(at_location.origin, knot);
    EXPECT_EQ(after_location.origin, knot);

    // Exercise x1-1 and x1 on the left segment, then x0 and x0+1 on the right. Locator ownership at the exact knot
    // belongs to the right segment, whose x==x0 identity returns its directly quantized g0.
    auto const left_before = spline.payload.segments[before_location.index](before, before_location.origin);
    auto const left_at_knot = spline.payload.segments[before_location.index](knot, before_location.origin);
    auto const right_at_knot = spline.payload.segments[at_location.index](knot, at_location.origin);
    auto const right_after = spline.payload.segments[after_location.index](after, after_location.origin);
    EXPECT_EQ(spline(before), left_before);
    EXPECT_EQ(spline(knot), right_at_knot);
    EXPECT_EQ(spline(after), right_after);

    auto const knot_gain = target.gain(from_fixed<scalar_t>(knot));
    EXPECT_NEAR(from_fixed<scalar_t>(left_at_knot), knot_gain, 3e-6);
    EXPECT_NEAR(from_fixed<scalar_t>(right_at_knot), knot_gain, 3e-6);

    auto const x_max = x_t{policy_t::domain_end};
    auto const inside = x_t::literal(x_max.value - 1);
    auto const final_location = spline.payload.segment_locator.locate(inside);
    auto const fixed_endpoint = spline.payload.segments[final_location.index](x_max, final_location.origin);

    EXPECT_EQ(spline.payload.extend_final_tangent.y0, fixed_endpoint);
    EXPECT_EQ(spline(x_max), fixed_endpoint);

    auto const first_beyond = x_t::literal(x_max.value + 1);
    EXPECT_EQ(spline(first_beyond), spline.payload.extend_final_tangent(x_t::literal(1)));
}

} // namespace
} // namespace crv::spline
