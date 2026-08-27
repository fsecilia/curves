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

TEST_F(authored_validator_test_t, rejects_unimplemented_shaping)
{
    auto& synchronous = std::get<0>(profile.curves.configs);
    synchronous.common.scale.input.value(2.0);
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

} // namespace
} // namespace crv::pipeline::configuration::construction
