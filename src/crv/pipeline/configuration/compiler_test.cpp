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
