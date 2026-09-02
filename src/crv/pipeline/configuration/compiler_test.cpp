// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "compiler.hpp"
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/test/test.hpp>
#include <array>
#include <variant>

namespace crv::pipeline::configuration {
namespace {

constexpr auto supported_curve_ids = [] {
    auto result = std::array<model::curves::curve_id_t, static_cast<std::size_t>(model::curves::curves_count)>{};
    for (auto index = std::size_t{}; index < result.size(); ++index)
    {
        result[index] = static_cast<model::curves::curve_id_t>(index);
    }
    return result;
}();

struct pipeline_compiler_curve_test_t : TestWithParam<model::curves::curve_id_t>
{
    model::device_t device;
    model::profile_t profile;
    compiler_t sut;

    pipeline_compiler_curve_test_t()
    {
        device.dpi.value(800);
        profile.curves.active.value(GetParam());
    }
};

TEST_P(pipeline_compiler_curve_test_t, supported_authored_curve_compiles_to_valid_runtime)
{
    EXPECT_TRUE(sut(device, profile));
}
INSTANTIATE_TEST_SUITE_P(supported_curves, pipeline_compiler_curve_test_t, ValuesIn(supported_curve_ids));

struct pipeline_compiler_test_t : Test
{
    model::device_t device;
    model::profile_t profile;
    compiler_t sut;

    pipeline_compiler_test_t() { device.dpi.value(800); }

    [[nodiscard]] static auto gain_at(runtime_t const& runtime, float_t input) -> float_t
    {
        auto hint = pipeline_t::gain_t::hint_t{};
        auto const x = to_fixed<pipeline_t::gain_t::x_t>(input);
        return from_fixed<float_t>(runtime.gain.evaluate(x, hint));
    }
};

TEST_F(pipeline_compiler_test_t, disabled_filter_reaches_runtime_zero)
{
    profile.filter_halflife.value(0.0);
    auto const result = sut(device, profile);
    ASSERT_TRUE(result);
    EXPECT_EQ(result->config.half_life, pipeline_t::duration_t{});
}

TEST_F(pipeline_compiler_test_t, output_dpi_reaches_runtime_scale)
{
    device.dpi.value(26'000);
    profile.output_dpi.value(1000);

    auto const result = sut(device, profile);

    EXPECT_TRUE(
        result && result->config.output_transform.output_scale == pipeline_t::output_scale_t::literal(82'595'525));
}

TEST_F(pipeline_compiler_test_t, high_anisotropy_rotated_transform_is_accepted_end_to_end)
{
    device.rotation.value(45.0);
    profile.anisotropy.value(1000.0);
    EXPECT_TRUE(sut(device, profile));
}

TEST_F(pipeline_compiler_test_t, authored_validation_is_available_without_compilation)
{
    device.dpi.value(0);
    EXPECT_EQ(sut.validate(device, profile).error, construction::authored_validation_error_t::dpi);
}

TEST_F(pipeline_compiler_test_t, zero_dpi_fails_before_numerical_construction)
{
    device.dpi.value(0);
    auto const result = sut(device, profile);
    ASSERT_FALSE(result);
    EXPECT_EQ(std::get<construction::authored_validation_result_t>(result.error()).error,
        construction::authored_validation_error_t::dpi);
}

TEST_F(pipeline_compiler_test_t, common_output_scale_reaches_runtime_gain)
{
    auto const baseline = sut(device, profile);
    ASSERT_TRUE(baseline);

    auto& synchronous = std::get<0>(profile.curves.configs);
    synchronous.common.scale.output.value(2.0);
    auto const scaled = sut(device, profile);
    ASSERT_TRUE(scaled);

    EXPECT_NEAR(gain_at(*scaled, 0.0), 2.0 * gain_at(*baseline, 0.0), 2e-6);
}

TEST_F(pipeline_compiler_test_t, relative_positioning_is_applied_after_common_output_scale)
{
    auto& synchronous = std::get<0>(profile.curves.configs);
    synchronous.common.scale.output.value(2.0);
    synchronous.common.anchor.height.value(0.5);

    auto const result = sut(device, profile);
    ASSERT_TRUE(result);

    auto const raw_origin = 1.0 / synchronous.specific.motivity.value();
    EXPECT_NEAR(gain_at(*result, 0.0), 2.0 * raw_origin + 0.5, 2e-6);
}

TEST_F(pipeline_compiler_test_t, fixed_anchor_is_unchanged_by_common_output_scale)
{
    auto& synchronous = std::get<0>(profile.curves.configs);
    synchronous.common.anchor.mode.value(model::anchor_mode_t::fixed);
    synchronous.common.anchor.height.value(1.25);

    synchronous.common.scale.output.value(2.0);
    auto const scale_two = sut(device, profile);
    ASSERT_TRUE(scale_two);

    synchronous.common.scale.output.value(4.0);
    auto const scale_four = sut(device, profile);
    ASSERT_TRUE(scale_four);

    EXPECT_NEAR(gain_at(*scale_two, 0.0), 1.25, 2e-6);
    EXPECT_NEAR(gain_at(*scale_four, 0.0), 1.25, 2e-6);
}

TEST_F(pipeline_compiler_test_t, smooth_gain_compiles_with_gain_semantics)
{
    profile.curves.active.value(model::curves::curve_id_t::smooth_gain);

    auto const result = sut(device, profile);
    ASSERT_TRUE(result);

    EXPECT_NEAR(gain_at(*result, 20.0), 1.5, 2e-6);
}

TEST_F(pipeline_compiler_test_t, synchronous_runtime_spline_keeps_quantized_sync_speed_as_knot)
{
    auto const result = sut(device, profile);
    ASSERT_TRUE(result);

    auto const& synchronous = std::get<0>(profile.curves.configs).specific;
    auto const sync_speed = to_fixed<typename decltype(result->gain)::x_t>(synchronous.sync_speed.value());
    EXPECT_EQ(result->gain.segment_locator.locate(sync_speed).origin, sync_speed);
}

} // namespace
} // namespace crv::pipeline::configuration
