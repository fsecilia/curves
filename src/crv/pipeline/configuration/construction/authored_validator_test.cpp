// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "authored_validator.hpp"
#include <crv/pipeline/configuration/construction/config_builder.hpp>
#include <crv/test/test.hpp>
#include <limits>

namespace crv::pipeline::configuration::construction {
namespace {

struct authored_validator_test_t : Test
{
    model::device_t device;
    model::profile_t profile;
    authored_validator_t<half_life_builder_t> sut{.build_half_life = half_life_builder_t{}};

    authored_validator_test_t() { device.dpi.value(800); }

    auto validate() const -> authored_validation_result_t { return sut(device, profile); }
};

TEST_F(authored_validator_test_t, accepts_current_nominal_model)
{
    EXPECT_TRUE(validate());
}

TEST_F(authored_validator_test_t, rejects_zero_dpi)
{
    device.dpi.value(0);
    EXPECT_EQ(validate().error, authored_validation_error_t::dpi);
}

TEST_F(authored_validator_test_t, rejects_nonpositive_output_dpi)
{
    profile.output_dpi.value(0);
    EXPECT_EQ(validate().error, authored_validation_error_t::output_dpi);
}

TEST_F(authored_validator_test_t, rejects_output_scale_beyond_runtime_capacity)
{
    device.dpi.value(1);
    profile.output_dpi.value(static_cast<int_t>(pipeline_t::max_output_scale) + 1);
    EXPECT_EQ(validate().error, authored_validation_error_t::output_dpi);
}

TEST_F(authored_validator_test_t, rejects_nonfinite_rotation)
{
    device.rotation.value(std::numeric_limits<float_t>::quiet_NaN());
    EXPECT_EQ(validate().error, authored_validation_error_t::rotation);
}

TEST_F(authored_validator_test_t, accepts_disabled_filter)
{
    profile.filter_halflife.value(0.0);
    EXPECT_TRUE(validate());
}

TEST_F(authored_validator_test_t, rejects_positive_half_life_that_rounds_to_zero)
{
    profile.filter_halflife.value(1e-13);
    EXPECT_EQ(validate().error, authored_validation_error_t::filter_half_life_underflow);
}

TEST_F(authored_validator_test_t, rejects_invalid_curve_id_before_tuple_dispatch)
{
    profile.curves.active.value(static_cast<model::curves::curve_id_t>(99));
    EXPECT_EQ(validate().error, authored_validation_error_t::curve_id);
}

TEST_F(authored_validator_test_t, rejects_invalid_curve_interpretation)
{
    auto& synchronous = std::get<0>(profile.curves.configs);
    synchronous.interpretation.value(static_cast<model::curve_interpretation_t>(99));
    EXPECT_EQ(validate().error, authored_validation_error_t::curve_interpretation);
}

TEST_F(authored_validator_test_t, rejects_unimplemented_shaping)
{
    auto& synchronous = std::get<0>(profile.curves.configs);
    synchronous.common.scale.input.value(2.0);
    EXPECT_EQ(validate().error, authored_validation_error_t::unsupported_shaping);
}

TEST_F(authored_validator_test_t, accepts_common_output_scale)
{
    auto& synchronous = std::get<0>(profile.curves.configs);
    synchronous.common.scale.output.value(2.0);
    EXPECT_TRUE(validate());
}

TEST_F(authored_validator_test_t, rejects_invalid_common_output_scale)
{
    auto& synchronous = std::get<0>(profile.curves.configs);
    synchronous.common.scale.output.value(std::numeric_limits<float_t>::quiet_NaN());
    EXPECT_EQ(validate().error, authored_validation_error_t::output_scale);
}

TEST_F(authored_validator_test_t, accepts_negative_relative_positioning_height)
{
    auto& synchronous = std::get<0>(profile.curves.configs);
    synchronous.common.anchor.height.value(-0.25);
    EXPECT_TRUE(validate());
}

TEST_F(authored_validator_test_t, accepts_nonnegative_fixed_anchor)
{
    auto& synchronous = std::get<0>(profile.curves.configs);
    synchronous.common.anchor.mode.value(model::anchor_mode_t::fixed);
    synchronous.common.anchor.height.value(0.25);
    EXPECT_TRUE(validate());
}

TEST_F(authored_validator_test_t, rejects_negative_fixed_anchor)
{
    auto& synchronous = std::get<0>(profile.curves.configs);
    synchronous.common.anchor.mode.value(model::anchor_mode_t::fixed);
    synchronous.common.anchor.height.value(-0.25);
    EXPECT_EQ(validate().error, authored_validation_error_t::fixed_anchor_negative);
}

TEST_F(authored_validator_test_t, rejects_invalid_positioning_height)
{
    auto& synchronous = std::get<0>(profile.curves.configs);
    synchronous.common.anchor.height.value(std::numeric_limits<float_t>::quiet_NaN());
    EXPECT_EQ(validate().error, authored_validation_error_t::positioning_height);
}

TEST_F(authored_validator_test_t, rejects_invalid_positioning_mode)
{
    auto& synchronous = std::get<0>(profile.curves.configs);
    synchronous.common.anchor.mode.value(static_cast<model::anchor_mode_t>(99));
    EXPECT_EQ(validate().error, authored_validation_error_t::positioning_mode);
}

TEST_F(authored_validator_test_t, still_rejects_unimplemented_ceiling)
{
    auto& synchronous = std::get<0>(profile.curves.configs);
    synchronous.common.ceiling.height.value(999.0);
    EXPECT_EQ(validate().error, authored_validation_error_t::unsupported_shaping);
}

TEST_F(authored_validator_test_t, accepts_synchronous_unit_motivity)
{
    auto& synchronous = std::get<0>(profile.curves.configs);
    synchronous.specific.motivity.value(1.0);
    EXPECT_TRUE(validate());
}

TEST_F(authored_validator_test_t, rejects_log_normal_zero_peak)
{
    profile.curves.active.value(model::curves::curve_id_t::log_normal);
    auto& log_normal = std::get<1>(profile.curves.configs);
    log_normal.specific.accel_peak.value(0.0);
    EXPECT_EQ(validate().error, authored_validation_error_t::log_normal_accel_peak);
}

TEST_F(authored_validator_test_t, rejects_log_normal_nonpositive_scale)
{
    profile.curves.active.value(model::curves::curve_id_t::log_normal);
    auto& log_normal = std::get<1>(profile.curves.configs);
    log_normal.specific.baseline.value(2.0);
    log_normal.specific.limit.value(1.0);
    EXPECT_EQ(validate().error, authored_validation_error_t::log_normal_limit);
}

TEST_F(authored_validator_test_t, accepts_smooth_gain_while_curve_specific_validation_is_deferred)
{
    profile.curves.active.value(model::curves::curve_id_t::smooth_gain);
    auto& smooth_gain = std::get<2>(profile.curves.configs);
    smooth_gain.specific.v_0.value(20.0);
    smooth_gain.specific.v_50.value(1.0);
    EXPECT_TRUE(validate());
}

} // namespace
} // namespace crv::pipeline::configuration::construction
