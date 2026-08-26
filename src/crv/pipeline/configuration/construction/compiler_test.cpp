// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "compiler.hpp"
#include <crv/pipeline/configuration/construction/authored_validator.hpp>
#include <crv/test/test.hpp>
#include <expected>
#include <gmock/gmock.h>
#include <variant>

namespace crv::pipeline::configuration {
namespace {

struct compiler_test_t : Test
{
    enum class gain_error_t
    {
        failed,
    };
    using gain_result_t = std::expected<void, gain_error_t>;

    struct mock_authored_validator_t
    {
        MOCK_METHOD(authored_validation_result_t, call, (model::device_t const&, model::profile_t const&), (const));
    };
    StrictMock<mock_authored_validator_t> mock_authored_validator;

    struct authored_validator_delegate_t
    {
        using result_t = authored_validation_result_t;
        mock_authored_validator_t* mock;

        auto operator()(model::device_t const& device, model::profile_t const& profile) const -> result_t
        {
            return mock->call(device, profile);
        }
    };

    struct mock_config_builder_t
    {
        MOCK_METHOD(pipeline_t::config_t, call, (model::device_t const&, model::profile_t const&), (const));
    };
    StrictMock<mock_config_builder_t> mock_config_builder;

    struct config_builder_delegate_t
    {
        mock_config_builder_t* mock;

        auto operator()(model::device_t const& device, model::profile_t const& profile) const -> pipeline_t::config_t
        {
            return mock->call(device, profile);
        }
    };

    struct mock_gain_compiler_t
    {
        MOCK_METHOD(gain_result_t, call, (pipeline_t::gain_t&, model::curves_t const&), (const));
    };
    StrictMock<mock_gain_compiler_t> mock_gain_compiler;

    struct gain_compiler_delegate_t
    {
        using error_t = gain_error_t;
        mock_gain_compiler_t* mock;

        auto operator()(pipeline_t::gain_t& gain, model::curves_t const& curves) const -> gain_result_t
        {
            return mock->call(gain, curves);
        }
    };

    struct mock_runtime_validator_t
    {
        MOCK_METHOD(
            pipeline_t::validation_result_t, call, (pipeline_t::config_t const&, pipeline_t::gain_t const&), (const));
    };
    StrictMock<mock_runtime_validator_t> mock_runtime_validator;

    struct runtime_validator_delegate_t
    {
        using result_t = pipeline_t::validation_result_t;
        mock_runtime_validator_t* mock;

        auto operator()(pipeline_t::config_t const& config, pipeline_t::gain_t const& gain) const -> result_t
        {
            return mock->call(config, gain);
        }
    };

    using sut_t = compiler_t<authored_validator_delegate_t, config_builder_delegate_t, gain_compiler_delegate_t,
        runtime_validator_delegate_t>;

    model::device_t device;
    model::profile_t profile;
    pipeline_t::config_t config{.velocity_scale = pipeline_t::velocity_scale_t{37}};
    sut_t sut{
        .validate_authored = {&mock_authored_validator},
        .build_config = {&mock_config_builder},
        .compile_gain = {&mock_gain_compiler},
        .validate_runtime = {&mock_runtime_validator},
    };
};

TEST_F(compiler_test_t, authored_failure_stops_before_runtime_construction)
{
    auto const failure = authored_validation_result_t{.error = authored_validation_error_t::dpi};
    EXPECT_CALL(mock_authored_validator, call(Ref(device), Ref(profile))).WillOnce(Return(failure));

    auto const result = sut(device, profile);

    EXPECT_EQ(std::get<authored_validation_result_t>(result.error()), failure);
}

TEST_F(compiler_test_t, gain_failure_is_preserved)
{
    EXPECT_CALL(mock_authored_validator, call).WillOnce(Return(authored_validation_result_t{}));
    EXPECT_CALL(mock_config_builder, call).WillOnce(Return(config));
    EXPECT_CALL(mock_gain_compiler, call).WillOnce(Return(std::unexpected{gain_error_t::failed}));

    auto const result = sut(device, profile);

    EXPECT_EQ(std::get<gain_error_t>(result.error()), gain_error_t::failed);
}

TEST_F(compiler_test_t, runtime_validation_failure_is_preserved)
{
    auto const failure
        = pipeline_t::validation_result_t{.error = pipeline::runtime_config_validation_error_t::spline_tangent};
    EXPECT_CALL(mock_authored_validator, call).WillOnce(Return(authored_validation_result_t{}));
    EXPECT_CALL(mock_config_builder, call).WillOnce(Return(config));
    EXPECT_CALL(mock_gain_compiler, call).WillOnce(Return(gain_result_t{}));
    EXPECT_CALL(mock_runtime_validator, call).WillOnce(Return(failure));

    auto const result = sut(device, profile);

    EXPECT_EQ(std::get<pipeline_t::validation_result_t>(result.error()), failure);
}

TEST_F(compiler_test_t, success_returns_constructed_runtime_value)
{
    EXPECT_CALL(mock_authored_validator, call).WillOnce(Return(authored_validation_result_t{}));
    EXPECT_CALL(mock_config_builder, call).WillOnce(Return(config));
    EXPECT_CALL(mock_gain_compiler, call).WillOnce(Return(gain_result_t{}));
    EXPECT_CALL(mock_runtime_validator, call).WillOnce(Return(pipeline_t::validation_result_t{}));

    auto const result = sut(device, profile);

    EXPECT_EQ(result->config.velocity_scale, config.velocity_scale);
}

} // namespace
} // namespace crv::pipeline::configuration
