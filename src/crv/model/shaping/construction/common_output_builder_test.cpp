// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "common_output_builder.hpp"
#include <crv/math/jet/jet.hpp>
#include <crv/test/test.hpp>
#include <cmath>
#include <limits>
#include <vector>

namespace crv::shaping::construction {
namespace {

struct common_output_builder_test_t : Test
{
    using scalar_t = float_t;
    using jet_t = crv::jet_t<scalar_t>;

    struct domain_t
    {
        scalar_t left{};
        scalar_t right{10};

        [[nodiscard]] auto contains(scalar_t input) const noexcept -> bool
        {
            return std::isfinite(input) && left <= input && input <= right;
        }
    };

    struct curve_t
    {
        using scalar_t = common_output_builder_test_t::scalar_t;
        using domain_t = common_output_builder_test_t::domain_t;

        scalar_t intercept{2};
        scalar_t slope{3};
        domain_t input_domain{};
        std::vector<scalar_t> points{4};

        [[nodiscard]] auto operator()(scalar_t input) const noexcept -> scalar_t { return intercept + slope * input; }
        [[nodiscard]] auto operator()(jet_t input) const noexcept -> jet_t { return intercept + slope * input; }
        [[nodiscard]] auto domain() const noexcept -> domain_t { return input_domain; }
        [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return points; }
    };

    model::common_curve_config_t config;
    common_output_builder_t sut;

    [[nodiscard]] auto build() { return sut(curve_t{}, config, scalar_t{10}); }

    [[nodiscard]] auto build(curve_t curve, scalar_t domain_end = 10)
    {
        return sut(std::move(curve), config, domain_end);
    }
};

TEST_F(common_output_builder_test_t, applies_output_scale_before_relative_offset)
{
    config.scale.output.value(2.0);
    config.anchor.height.value(5.0);

    auto const curve = build().value();

    EXPECT_EQ(curve(4.0), 33.0);
}

TEST_F(common_output_builder_test_t, output_scale_does_not_scale_relative_offset)
{
    config.scale.output.value(3.0);
    config.anchor.height.value(5.0);

    auto const curve = build().value();

    EXPECT_EQ(curve(0.0), 11.0);
}

TEST_F(common_output_builder_test_t, fixed_anchor_maps_origin_exactly_to_requested_height)
{
    config.scale.output.value(3.0);
    config.anchor.mode.value(model::anchor_mode_t::fixed);
    config.anchor.height.value(5.0);

    auto const curve = build().value();

    EXPECT_EQ(curve(0.0), 5.0);
}

TEST_F(common_output_builder_test_t, changing_scale_does_not_move_fixed_anchor)
{
    config.anchor.mode.value(model::anchor_mode_t::fixed);
    config.anchor.height.value(5.0);

    config.scale.output.value(2.0);
    auto const scale_two = build().value();

    config.scale.output.value(4.0);
    auto const scale_four = build().value();

    EXPECT_EQ(scale_two(0.0), scale_four(0.0));
}

TEST_F(common_output_builder_test_t, changing_scale_changes_curve_away_from_fixed_anchor)
{
    config.anchor.mode.value(model::anchor_mode_t::fixed);
    config.anchor.height.value(5.0);

    config.scale.output.value(2.0);
    auto const scale_two = build().value();

    config.scale.output.value(4.0);
    auto const scale_four = build().value();

    EXPECT_NE(scale_two(1.0), scale_four(1.0));
}

TEST_F(common_output_builder_test_t, fixed_anchor_preserves_small_height_against_large_scaled_origin)
{
    config.scale.output.value(999.0);
    config.anchor.mode.value(model::anchor_mode_t::fixed);
    config.anchor.height.value(1e-12);

    auto const curve = build(curve_t{.intercept = 1000.0, .slope = 0.0}).value();

    EXPECT_EQ(curve(0.0), config.anchor.height.value());
}

TEST_F(common_output_builder_test_t, positioning_leaves_jet_tangent_after_scale_unchanged)
{
    config.scale.output.value(2.0);
    config.anchor.mode.value(model::anchor_mode_t::fixed);
    config.anchor.height.value(7.0);

    auto const curve = build().value();

    EXPECT_EQ(curve(jet_t{4.0, 5.0}).df, 30.0);
}

TEST_F(common_output_builder_test_t, forwards_critical_points)
{
    auto const curve = build().value();
    EXPECT_EQ(curve.critical_points(), (std::vector<scalar_t>{4.0}));
}

TEST_F(common_output_builder_test_t, forwards_nested_domain)
{
    auto const curve = build().value();
    EXPECT_TRUE(curve.domain().contains(10.0));
}

TEST_F(common_output_builder_test_t, rejects_nonfinite_output_scale)
{
    config.scale.output.value(std::numeric_limits<float_t>::quiet_NaN());
    EXPECT_EQ(build().error(), common_output_error_t::output_scale_not_finite);
}

TEST_F(common_output_builder_test_t, rejects_nonfinite_positioning_height)
{
    config.anchor.height.value(std::numeric_limits<float_t>::quiet_NaN());
    EXPECT_EQ(build().error(), common_output_error_t::positioning_height_not_finite);
}

TEST_F(common_output_builder_test_t, rejects_negative_fixed_anchor)
{
    config.anchor.mode.value(model::anchor_mode_t::fixed);
    config.anchor.height.value(-1.0);
    EXPECT_EQ(build().error(), common_output_error_t::fixed_anchor_negative);
}

TEST_F(common_output_builder_test_t, rejects_relative_offset_that_makes_origin_negative)
{
    config.anchor.height.value(-3.0);
    EXPECT_EQ(build().error(), common_output_error_t::positioned_origin_negative);
}

TEST_F(common_output_builder_test_t, accepts_negative_relative_offset_when_origin_remains_nonnegative)
{
    config.anchor.height.value(-1.0);
    EXPECT_TRUE(build());
}

TEST_F(common_output_builder_test_t, rejects_origin_outside_nested_domain)
{
    auto const curve = curve_t{.input_domain = {.left = 1.0, .right = 10.0}};
    EXPECT_EQ(build(curve).error(), common_output_error_t::origin_outside_domain);
}

TEST_F(common_output_builder_test_t, rejects_domain_end_outside_nested_domain)
{
    auto const curve = curve_t{.input_domain = {.left = 0.0, .right = 9.0}};
    EXPECT_EQ(build(curve).error(), common_output_error_t::domain_end_outside_domain);
}

TEST_F(common_output_builder_test_t, rejects_scale_overflow_at_origin)
{
    config.scale.output.value(2.0);
    auto const max = std::numeric_limits<scalar_t>::max();
    auto const curve = curve_t{.intercept = max, .slope = 0.0};

    EXPECT_EQ(build(curve).error(), common_output_error_t::scaled_origin_not_representable);
}

TEST_F(common_output_builder_test_t, rejects_scale_overflow_at_domain_end)
{
    config.scale.output.value(2.0);
    auto const max = std::numeric_limits<scalar_t>::max();
    auto const curve = curve_t{.intercept = max / 2.0, .slope = max / 20.0};

    EXPECT_EQ(build(curve).error(), common_output_error_t::scaled_domain_end_not_representable);
}

} // namespace
} // namespace crv::shaping::construction
