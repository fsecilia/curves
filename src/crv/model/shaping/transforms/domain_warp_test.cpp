// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "domain_warp.hpp"
#include <crv/model/domain.hpp>
#include <crv/model/shaping/transitions/smoothstep.hpp>
#include <crv/test/test.hpp>
#include <cmath>
#include <gmock/gmock.h>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace crv::shaping::transforms {
namespace {

using scalar_t = float_t;
using jet_t = crv::jet_t<scalar_t>;
using transition_t = transitions::smoothstep_t;
using sut_t = domain_warp_t<scalar_t, transition_t>;

[[nodiscard]] auto construct_sut(scalar_t hold_width, scalar_t transition_width) -> sut_t
{
    return std::move(sut_t::construct(hold_width, transition_width, transition_t{})).value();
}

TEST(shaping_transforms_domain_warp_parameter_test_t, rejects_negative_hold_width)
{
    EXPECT_EQ(sut_t::construct(-1.0, 1.0, transition_t{}), std::unexpected{domain_warp_error_t::hold_width_negative});
}

TEST(shaping_transforms_domain_warp_parameter_test_t, rejects_nonfinite_hold_width)
{
    EXPECT_EQ(sut_t::construct(std::numeric_limits<scalar_t>::infinity(), 1.0, transition_t{}),
        std::unexpected{domain_warp_error_t::hold_width_not_finite});
}

TEST(shaping_transforms_domain_warp_parameter_test_t, rejects_negative_transition_width)
{
    EXPECT_EQ(sut_t::construct(1.0, -1.0, transition_t{}), std::unexpected{domain_warp_error_t::transition_width_negative});
}

TEST(shaping_transforms_domain_warp_parameter_test_t, rejects_nonfinite_transition_width)
{
    EXPECT_EQ(sut_t::construct(1.0, std::numeric_limits<scalar_t>::quiet_NaN(), transition_t{}),
        std::unexpected{domain_warp_error_t::transition_width_not_finite});
}

TEST(shaping_transforms_domain_warp_parameter_test_t, accepts_zero_hold_width)
{
    EXPECT_TRUE(sut_t::construct(0.0, 1.0, transition_t{}));
}

TEST(shaping_transforms_domain_warp_parameter_test_t, accepts_zero_transition_width)
{
    EXPECT_TRUE(sut_t::construct(1.0, 0.0, transition_t{}));
}

TEST(shaping_transforms_domain_warp_support_test_t, accepts_exactly_representable_positive_support_endpoint)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    auto const hold_width = max / scalar_t{2};
    auto const transition_width = max - hold_width;
    EXPECT_TRUE(sut_t::construct(hold_width, transition_width, transition_t{}));
}

TEST(shaping_transforms_domain_warp_support_test_t, accepts_support_endpoint_when_actual_sum_rounds_to_maximum)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    auto const hold_width = max * scalar_t{13.0 / 16.0};
    auto const transition_width = std::nextafter(max - hold_width, max);
    EXPECT_TRUE(sut_t::construct(hold_width, transition_width, transition_t{}));
}

TEST(shaping_transforms_domain_warp_support_test_t, rejects_overflowing_positive_support_endpoint)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    EXPECT_EQ(sut_t::construct(max, max, transition_t{}), std::unexpected{domain_warp_error_t::support_not_representable});
}

TEST(shaping_transforms_domain_warp_support_test_t, rejects_positive_width_that_rounds_back_to_hold_width)
{
    auto const hold_width = scalar_t{1};
    auto const transition_width = std::numeric_limits<scalar_t>::denorm_min();
    EXPECT_EQ(sut_t::construct(hold_width, transition_width, transition_t{}),
        std::unexpected{domain_warp_error_t::support_not_representable});
}

struct endpoint_transition_t
{
    scalar_t endpoint_integral;

    [[nodiscard]] auto value(scalar_t u) const noexcept -> scalar_t { return u; }
    [[nodiscard]] auto derivative(scalar_t) const noexcept -> scalar_t { return scalar_t{1}; }
    [[nodiscard]] auto antiderivative(scalar_t u) const noexcept -> scalar_t
    {
        if (u >= scalar_t{1}) return endpoint_integral;
        return endpoint_integral * u;
    }
    [[nodiscard]] auto value(jet_t u) const noexcept -> jet_t { return {value(primal(u)), tangent(u)}; }
    [[nodiscard]] auto antiderivative(jet_t u) const noexcept -> jet_t
    {
        return {antiderivative(primal(u)), endpoint_integral * tangent(u)};
    }
};

using endpoint_sut_t = domain_warp_t<scalar_t, endpoint_transition_t>;

TEST(shaping_transforms_domain_warp_transition_geometry_test_t, rejects_nonfinite_endpoint_integral)
{
    auto const transition = endpoint_transition_t{std::numeric_limits<scalar_t>::infinity()};
    EXPECT_EQ(endpoint_sut_t::construct(1.0, 1.0, transition),
        std::unexpected{domain_warp_error_t::transition_endpoint_integral_not_finite});
}

TEST(shaping_transforms_domain_warp_transition_geometry_test_t, rejects_nonpositive_endpoint_integral)
{
    EXPECT_EQ(endpoint_sut_t::construct(1.0, 1.0, endpoint_transition_t{0.0}),
        std::unexpected{domain_warp_error_t::transition_endpoint_integral_not_positive});
}

TEST(shaping_transforms_domain_warp_transition_geometry_test_t, rejects_endpoint_integral_above_one)
{
    EXPECT_EQ(endpoint_sut_t::construct(1.0, 1.0, endpoint_transition_t{1.5}),
        std::unexpected{domain_warp_error_t::transition_endpoint_integral_above_one});
}

struct dead_transition_t
{
    struct state_t
    {
        int_t calls{};
    };

    state_t* state;

    [[nodiscard]] auto value(scalar_t) const noexcept -> scalar_t
    {
        ++state->calls;
        return scalar_t{};
    }
    [[nodiscard]] auto derivative(scalar_t) const noexcept -> scalar_t
    {
        ++state->calls;
        return scalar_t{};
    }
    [[nodiscard]] auto antiderivative(scalar_t) const noexcept -> scalar_t
    {
        ++state->calls;
        return scalar_t{};
    }
    [[nodiscard]] auto value(jet_t) const noexcept -> jet_t
    {
        ++state->calls;
        return {};
    }
    [[nodiscard]] auto antiderivative(jet_t) const noexcept -> jet_t
    {
        ++state->calls;
        return {};
    }
};

TEST(shaping_transforms_domain_warp_transition_geometry_test_t, zero_transition_width_does_not_query_dead_transition)
{
    auto state = dead_transition_t::state_t{};
    [[maybe_unused]] auto const result
        = domain_warp_t<scalar_t, dead_transition_t>::construct(1.0, 0.0, dead_transition_t{&state});
    EXPECT_EQ(state.calls, int_t{0});
}

TEST(shaping_transforms_domain_warp_scalar_test_t, input_below_hold_maps_to_exact_zero)
{
    EXPECT_EQ(construct_sut(2.0, 4.0).apply(1.0), 0.0);
}

TEST(shaping_transforms_domain_warp_scalar_test_t, input_at_hold_boundary_maps_to_exact_zero)
{
    EXPECT_EQ(construct_sut(2.0, 4.0).apply(2.0), 0.0);
}

TEST(shaping_transforms_domain_warp_scalar_test_t, transition_uses_scaled_antiderivative)
{
    EXPECT_EQ(construct_sut(2.0, 4.0).apply(4.0), 4.0 * transition_t{}.antiderivative(0.5));
}

TEST(shaping_transforms_domain_warp_scalar_test_t, support_endpoint_returns_exact_transition_endpoint_output)
{
    EXPECT_EQ(construct_sut(2.0, 4.0).apply(6.0), 2.0);
}

TEST(shaping_transforms_domain_warp_scalar_test_t, progression_preserves_symmetric_half_width_lag)
{
    EXPECT_EQ(construct_sut(2.0, 4.0).apply(10.0), 6.0);
}

TEST(shaping_transforms_domain_warp_scalar_test_t, zero_transition_width_holds_through_boundary)
{
    EXPECT_EQ(construct_sut(2.0, 0.0).apply(2.0), 0.0);
}

TEST(shaping_transforms_domain_warp_scalar_test_t, zero_transition_width_releases_immediately_after_boundary)
{
    auto const input = std::nextafter(scalar_t{2}, std::numeric_limits<scalar_t>::infinity());
    EXPECT_EQ(construct_sut(2.0, 0.0).apply(input), input - scalar_t{2});
}

TEST(shaping_transforms_domain_warp_scalar_test_t, zero_hold_and_transition_is_ordinary_progression_for_positive_input)
{
    EXPECT_EQ(construct_sut(0.0, 0.0).apply(3.0), 3.0);
}

TEST(shaping_transforms_domain_warp_scalar_test_t, zero_hold_begins_transition_at_origin)
{
    EXPECT_EQ(construct_sut(0.0, 4.0).apply(2.0), 4.0 * transition_t{}.antiderivative(0.5));
}

TEST(shaping_transforms_domain_warp_scalar_test_t, non_symmetric_endpoint_integral_derives_continuous_lag)
{
    auto const sut = endpoint_sut_t::construct(2.0, 4.0, endpoint_transition_t{0.25}).value();
    EXPECT_EQ(sut.apply(10.0), 5.0);
}

TEST(shaping_transforms_domain_warp_domain_test_t, rejects_nonfinite_transform_input)
{
    EXPECT_FALSE(construct_sut(1.0, 1.0).try_apply(std::numeric_limits<scalar_t>::infinity()));
}

TEST(shaping_transforms_domain_warp_jet_test_t, hold_has_exact_zero_tangent)
{
    EXPECT_EQ(construct_sut(2.0, 4.0).apply(jet_t{1.0, 7.0}), (jet_t{0.0, 0.0}));
}

TEST(shaping_transforms_domain_warp_jet_test_t, transition_uses_analytic_transition_value_for_tangent)
{
    auto const actual = construct_sut(2.0, 4.0).apply(jet_t{4.0, 7.0});
    EXPECT_EQ(actual.df, transition_t{}.value(0.5) * 7.0);
}

TEST(shaping_transforms_domain_warp_jet_test_t, support_endpoint_uses_progression_tangent)
{
    EXPECT_EQ(construct_sut(2.0, 4.0).apply(jet_t{6.0, 7.0}), (jet_t{2.0, 7.0}));
}

TEST(shaping_transforms_domain_warp_jet_test_t, post_transition_preserves_input_tangent)
{
    EXPECT_EQ(construct_sut(2.0, 4.0).apply(jet_t{10.0, 7.0}), (jet_t{6.0, 7.0}));
}

TEST(shaping_transforms_domain_warp_jet_test_t, tiny_transition_width_avoids_reciprocal_tangent_overflow)
{
    auto const width = std::numeric_limits<scalar_t>::min();
    auto const input = jet_t{width / scalar_t{2}, std::numeric_limits<scalar_t>::max()};
    auto const actual = construct_sut(0.0, width).apply(input);
    EXPECT_TRUE(std::isfinite(actual.df));
}

struct shaping_transforms_domain_warp_curve_apply_test_t : Test
{
    struct mock_curve_t
    {
        virtual ~mock_curve_t() = default;
        MOCK_METHOD(scalar_t, scalar, (scalar_t), (const, noexcept));
        MOCK_METHOD(jet_t, jet, (jet_t), (const, noexcept));
    };
    StrictMock<mock_curve_t> mock_curve;

    struct curve_t
    {
        mock_curve_t* mock;
        [[nodiscard]] auto operator()(scalar_t input) const noexcept -> scalar_t { return mock->scalar(input); }
        [[nodiscard]] auto operator()(jet_t input) const noexcept -> jet_t { return mock->jet(input); }
    };
};

TEST_F(shaping_transforms_domain_warp_curve_apply_test_t,
    exact_hold_jet_evaluates_nested_scalar_at_zero_without_nested_jet)
{
    auto const sut = construct_sut(2.0, 4.0);
    EXPECT_CALL(mock_curve, scalar(scalar_t{0})).WillOnce(Return(scalar_t{5}));
    EXPECT_EQ(sut.apply(curve_t{&mock_curve}, jet_t{1.0, 7.0}), (jet_t{5.0, 0.0}));
}

struct zero_interior_transition_t
{
    [[nodiscard]] auto value(scalar_t u) const noexcept -> scalar_t { return u == scalar_t{0.5} ? scalar_t{0} : u; }
    [[nodiscard]] auto derivative(scalar_t) const noexcept -> scalar_t { return scalar_t{}; }
    [[nodiscard]] auto antiderivative(scalar_t u) const noexcept -> scalar_t
    {
        if (u == scalar_t{0.5}) return scalar_t{0};
        if (u >= scalar_t{1}) return scalar_t{0.5};
        return u / scalar_t{2};
    }
    [[nodiscard]] auto value(jet_t u) const noexcept -> jet_t { return {value(primal(u)), scalar_t{0} * tangent(u)}; }
    [[nodiscard]] auto antiderivative(jet_t u) const noexcept -> jet_t
    {
        return {antiderivative(primal(u)), value(primal(u)) * tangent(u)};
    }
};

TEST_F(shaping_transforms_domain_warp_curve_apply_test_t,
    interior_floating_zero_does_not_reclassify_transition_as_exact_hold)
{
    using zero_sut_t = domain_warp_t<scalar_t, zero_interior_transition_t>;
    auto const sut = zero_sut_t::construct(1.0, 2.0, zero_interior_transition_t{}).value();
    auto const input = jet_t{2.0, 7.0};
    EXPECT_CALL(mock_curve, jet(jet_t{0.0, 0.0})).WillOnce(Return(jet_t{3.0, 11.0}));
    EXPECT_EQ(sut.apply(curve_t{&mock_curve}, input), (jet_t{3.0, 11.0}));
}

TEST(shaping_transforms_domain_warp_domain_preimage_test_t, preserves_empty_nested_domain)
{
    EXPECT_TRUE(construct_sut(2.0, 4.0).preimage(model::input_domain_t<scalar_t>{}).empty());
}

TEST(shaping_transforms_domain_warp_domain_preimage_test_t, full_nested_domain_keeps_full_outer_domain)
{
    EXPECT_EQ(
        construct_sut(2.0, 4.0).preimage(model::input_domain_t<scalar_t>::full()), model::input_domain_t<scalar_t>::full());
}

TEST(shaping_transforms_domain_warp_domain_preimage_test_t, domain_entirely_below_zero_is_unreachable)
{
    EXPECT_TRUE(construct_sut(2.0, 4.0).preimage({-2.0, -1.0}).empty());
}

TEST(shaping_transforms_domain_warp_domain_preimage_test_t, zero_only_domain_ends_at_actual_hold_frontier)
{
    auto const sut = construct_sut(2.0, 4.0);
    auto const nested = model::input_domain_t<scalar_t>{0.0, 0.0};
    auto const domain = sut.preimage(nested);
    auto const successor = std::nextafter(domain.last(), std::numeric_limits<scalar_t>::infinity());

    EXPECT_EQ(domain.first(), std::numeric_limits<scalar_t>::lowest());
    EXPECT_TRUE(nested.contains(sut.apply(domain.last())));
    EXPECT_FALSE(nested.contains(sut.apply(successor)));
}

TEST(shaping_transforms_domain_warp_domain_preimage_test_t, lower_endpoint_inside_transition_is_exact)
{
    auto const sut = construct_sut(2.0, 4.0);
    auto const nested = model::input_domain_t<scalar_t>{0.375, 4.0};
    auto const domain = sut.preimage(nested);
    auto const predecessor = std::nextafter(domain.first(), -std::numeric_limits<scalar_t>::infinity());

    EXPECT_TRUE(nested.contains(sut.apply(domain.first())));
    EXPECT_FALSE(nested.contains(sut.apply(predecessor)));
}

TEST(shaping_transforms_domain_warp_domain_preimage_test_t, lower_endpoint_at_release_is_exact)
{
    auto const sut = construct_sut(2.0, 4.0);
    auto const nested = model::input_domain_t<scalar_t>{2.0, 4.0};
    auto const domain = sut.preimage(nested);
    auto const predecessor = std::nextafter(domain.first(), -std::numeric_limits<scalar_t>::infinity());

    EXPECT_TRUE(nested.contains(sut.apply(domain.first())));
    EXPECT_FALSE(nested.contains(sut.apply(predecessor)));
}

TEST(shaping_transforms_domain_warp_domain_preimage_test_t, lower_endpoint_after_release_is_exact)
{
    auto const sut = construct_sut(2.0, 4.0);
    auto const nested = model::input_domain_t<scalar_t>{3.0, 4.0};
    auto const domain = sut.preimage(nested);
    auto const predecessor = std::nextafter(domain.first(), -std::numeric_limits<scalar_t>::infinity());

    EXPECT_TRUE(nested.contains(sut.apply(domain.first())));
    EXPECT_FALSE(nested.contains(sut.apply(predecessor)));
}

TEST(shaping_transforms_domain_warp_domain_preimage_test_t, upper_endpoint_inside_transition_is_exact)
{
    auto const sut = construct_sut(2.0, 4.0);
    auto const nested = model::input_domain_t<scalar_t>{0.0, 0.375};
    auto const domain = sut.preimage(nested);
    auto const successor = std::nextafter(domain.last(), std::numeric_limits<scalar_t>::infinity());

    EXPECT_TRUE(nested.contains(sut.apply(domain.last())));
    EXPECT_FALSE(nested.contains(sut.apply(successor)));
}

TEST(shaping_transforms_domain_warp_domain_preimage_test_t, upper_endpoint_at_release_is_exact)
{
    auto const sut = construct_sut(2.0, 4.0);
    auto const nested = model::input_domain_t<scalar_t>{0.0, 2.0};
    auto const domain = sut.preimage(nested);
    auto const successor = std::nextafter(domain.last(), std::numeric_limits<scalar_t>::infinity());

    EXPECT_TRUE(nested.contains(sut.apply(domain.last())));
    EXPECT_FALSE(nested.contains(sut.apply(successor)));
}

TEST(shaping_transforms_domain_warp_domain_preimage_test_t, upper_endpoint_after_release_is_exact)
{
    auto const sut = construct_sut(2.0, 4.0);
    auto const nested = model::input_domain_t<scalar_t>{0.0, 3.0};
    auto const domain = sut.preimage(nested);
    auto const successor = std::nextafter(domain.last(), std::numeric_limits<scalar_t>::infinity());

    EXPECT_TRUE(nested.contains(sut.apply(domain.last())));
    EXPECT_FALSE(nested.contains(sut.apply(successor)));
}

TEST(shaping_transforms_domain_warp_domain_preimage_test_t, zero_transition_width_resolves_delayed_progression)
{
    auto const sut = construct_sut(2.0, 0.0);
    auto const nested = model::input_domain_t<scalar_t>{0.0, 3.0};
    auto const domain = sut.preimage(nested);
    auto const successor = std::nextafter(domain.last(), std::numeric_limits<scalar_t>::infinity());

    EXPECT_EQ(domain.first(), std::numeric_limits<scalar_t>::lowest());
    EXPECT_TRUE(nested.contains(sut.apply(domain.last())));
    EXPECT_FALSE(nested.contains(sut.apply(successor)));
}

TEST(shaping_transforms_domain_warp_domain_preimage_test_t, zero_transition_width_skipped_singleton_is_unreachable)
{
    auto const denorm = std::numeric_limits<scalar_t>::denorm_min();
    EXPECT_TRUE(construct_sut(2.0, 0.0).preimage({denorm, denorm}).empty());
}

TEST(shaping_transforms_domain_warp_domain_preimage_test_t, positive_domain_above_maximum_warp_output_is_unreachable)
{
    auto const max = std::numeric_limits<scalar_t>::max();
    auto const hold_width = max / scalar_t{4};
    auto const transition_width = max / scalar_t{4};
    EXPECT_TRUE(construct_sut(hold_width, transition_width).preimage({max, max}).empty());
}

TEST(shaping_transforms_domain_warp_domain_preimage_test_t, non_symmetric_transition_uses_actual_release_geometry)
{
    auto const sut = endpoint_sut_t::construct(2.0, 4.0, endpoint_transition_t{0.25}).value();
    auto const nested = model::input_domain_t<scalar_t>{0.0, 1.0};
    auto const domain = sut.preimage(nested);
    auto const successor = std::nextafter(domain.last(), std::numeric_limits<scalar_t>::infinity());

    EXPECT_TRUE(nested.contains(sut.apply(domain.last())));
    EXPECT_FALSE(nested.contains(sut.apply(successor)));
}

TEST(shaping_transforms_domain_warp_critical_points_test_t, zero_transition_width_contributes_hold_boundary_only)
{
    EXPECT_EQ(construct_sut(2.0, 0.0).critical_points(), (std::vector<scalar_t>{2.0}));
}

TEST(shaping_transforms_domain_warp_critical_points_test_t, positive_transition_width_contributes_both_boundaries)
{
    EXPECT_EQ(construct_sut(2.0, 4.0).critical_points(), (std::vector<scalar_t>{2.0, 6.0}));
}

TEST(shaping_transforms_domain_warp_preimage_test_t, negative_nested_point_is_unreachable)
{
    EXPECT_FALSE(construct_sut(2.0, 4.0).try_preimage_critical_point(-1.0));
}

TEST(shaping_transforms_domain_warp_preimage_test_t, zero_nested_point_has_no_unique_preimage)
{
    EXPECT_FALSE(construct_sut(2.0, 4.0).try_preimage_critical_point(0.0));
}

TEST(shaping_transforms_domain_warp_preimage_test_t, transition_point_is_inverted_through_antiderivative)
{
    auto const nested_point = scalar_t{4} * transition_t{}.antiderivative(scalar_t{0.5});
    EXPECT_EQ(construct_sut(2.0, 4.0).try_preimage_critical_point(nested_point), scalar_t{4});
}

TEST(shaping_transforms_domain_warp_preimage_test_t, transition_output_endpoint_returns_exact_support_endpoint)
{
    EXPECT_EQ(construct_sut(2.0, 4.0).try_preimage_critical_point(2.0), scalar_t{6});
}

TEST(shaping_transforms_domain_warp_preimage_test_t, post_transition_point_uses_translated_inverse)
{
    EXPECT_EQ(construct_sut(2.0, 4.0).try_preimage_critical_point(3.0), scalar_t{7});
}

TEST(shaping_transforms_domain_warp_preimage_test_t, zero_width_positive_point_uses_delayed_translation)
{
    EXPECT_EQ(construct_sut(2.0, 0.0).try_preimage_critical_point(3.0), scalar_t{5});
}

TEST(shaping_transforms_domain_warp_preimage_test_t, unrepresentable_post_transition_preimage_is_omitted)
{
    EXPECT_FALSE(construct_sut(1.0, 2.0).try_preimage_critical_point(std::numeric_limits<scalar_t>::max()));
}

} // namespace
} // namespace crv::shaping::transforms
