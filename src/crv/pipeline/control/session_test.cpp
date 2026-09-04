// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "session.hpp"
#include <crv/pipeline.hpp>
#include <crv/test/test.hpp>
#include <expected>
#include <gmock/gmock.h>
#include <vector>

namespace crv::pipeline::control {
namespace {

using namespace testing;

struct session_open_test_t : Test
{
    struct open_compiler_t
    {
        enum class error_t : uint8_t
        {
            unused,
        };
        using result_t = std::expected<configuration::runtime_t, error_t>;

        auto operator()(model::device_t const&, model::profile_t const&) const -> result_t
        {
            return std::unexpected{error_t::unused};
        }
    };

    struct successful_open_client_t
    {
        using devices_result_t = std::expected<std::vector<attachment_t>, error_t>;
        using apply_result_t = std::expected<void, error_t>;
        using open_result_t = std::expected<successful_open_client_t, error_t>;

        [[nodiscard]] static auto open() -> open_result_t { return successful_open_client_t{}; }
        auto devices() const -> devices_result_t { return std::vector<attachment_t>{}; }
        auto apply(attachment_id_t, configuration::runtime_t const&, configuration::apply_mode_t) const
            -> apply_result_t
        {
            return {};
        }
    };

    struct failing_open_client_t
    {
        using devices_result_t = std::expected<std::vector<attachment_t>, error_t>;
        using apply_result_t = std::expected<void, error_t>;
        using open_result_t = std::expected<failing_open_client_t, error_t>;

        static constexpr auto open_error = error_t{.code = error_code_t::endpoint_open_failed, .native_error = EACCES};

        [[nodiscard]] static auto open() -> open_result_t { return std::unexpected{open_error}; }
        auto devices() const -> devices_result_t { return std::vector<attachment_t>{}; }
        auto apply(attachment_id_t, configuration::runtime_t const&, configuration::apply_mode_t) const
            -> apply_result_t
        {
            return {};
        }
    };
};

struct session_test_t : Test
{
    enum class compiler_error_t : uint8_t
    {
        invalid_authored,
    };

    using compiler_result_t = std::expected<configuration::runtime_t, compiler_error_t>;
    using devices_result_t = std::expected<std::vector<attachment_t>, error_t>;
    using apply_result_t = std::expected<void, error_t>;

    struct mock_compiler_t
    {
        MOCK_METHOD(compiler_result_t, call, (model::device_t const&, model::profile_t const&), (const));
    };

    struct compiler_t
    {
        using error_t = compiler_error_t;
        using result_t = compiler_result_t;
        mock_compiler_t* mock;

        auto operator()(model::device_t const& device, model::profile_t const& profile) const -> result_t
        {
            return mock->call(device, profile);
        }
    };

    struct mock_client_t
    {
        MOCK_METHOD(devices_result_t, devices, (), (const));
        MOCK_METHOD(apply_result_t, apply,
            (attachment_id_t, configuration::runtime_t const&, configuration::apply_mode_t), (const));
    };

    struct client_t
    {
        using devices_result_t = session_test_t::devices_result_t;
        using apply_result_t = session_test_t::apply_result_t;
        using open_result_t = std::expected<client_t, error_t>;
        mock_client_t* mock;

        auto devices() const -> devices_result_t { return mock->devices(); }
        auto apply(attachment_id_t attachment, configuration::runtime_t const& runtime,
            configuration::apply_mode_t mode) const -> apply_result_t
        {
            return mock->apply(attachment, runtime, mode);
        }
    };

    using sut_t = generic::session_t<compiler_t, client_t>;

    StrictMock<mock_compiler_t> mock_compiler;
    StrictMock<mock_client_t> mock_client;
    model::device_t device;
    model::profile_t profile;
    configuration::runtime_t runtime;
    sut_t sut{{&mock_compiler}, {&mock_client}};

    session_test_t()
    {
        runtime.config.velocity_scale = pipeline_t::velocity_scale_t::literal(0x123456789);
        runtime.config.half_life = pipeline_t::duration_t::literal(0x23456789);
    }
};

TEST_F(session_test_t, devices_preserves_client_result)
{
    auto const attachments = std::vector<attachment_t>{
        attachment_t{.id = attachment_id_t{7}, .sysname = "input7"},
        attachment_t{.id = attachment_id_t{23}, .sysname = "input23"},
    };
    EXPECT_CALL(mock_client, devices).WillOnce(Return(attachments));

    auto const result = sut.devices();

    EXPECT_EQ(*result, attachments);
}

TEST_F(session_test_t, devices_preserves_client_error)
{
    auto const expected = error_t{.code = error_code_t::enumeration_failed, .native_error = EIO};
    EXPECT_CALL(mock_client, devices).WillOnce(Return(std::unexpected{expected}));

    auto const result = sut.devices();

    EXPECT_EQ(result.error(), expected);
}

TEST_F(session_test_t, active_apply_compiles_supplied_authored_state_and_forwards_runtime_attachment_and_mode)
{
    auto const attachment = attachment_id_t{41};
    auto runtime_matches = false;
    EXPECT_CALL(mock_compiler, call(Ref(device), Ref(profile))).WillOnce(Return(runtime));
    EXPECT_CALL(mock_client, apply(attachment, _, configuration::apply_mode_t::active))
        .WillOnce([&](attachment_id_t, configuration::runtime_t const& actual, configuration::apply_mode_t) {
            runtime_matches = actual.config.velocity_scale == runtime.config.velocity_scale
                && actual.config.half_life == runtime.config.half_life;
            return apply_result_t{};
        });

    auto const result = sut.apply(device, profile, attachment, configuration::apply_mode_t::active);

    EXPECT_TRUE(result && runtime_matches);
}

TEST_F(session_test_t, bypassed_apply_preserves_bypassed_mode)
{
    EXPECT_CALL(mock_compiler, call).WillOnce(Return(runtime));
    EXPECT_CALL(mock_client, apply(attachment_id_t{11}, _, configuration::apply_mode_t::bypassed))
        .WillOnce(Return(apply_result_t{}));

    auto const result = sut.apply(device, profile, attachment_id_t{11}, configuration::apply_mode_t::bypassed);

    EXPECT_TRUE(result.has_value());
}

TEST_F(session_test_t, compiler_failure_is_preserved_without_control_apply)
{
    EXPECT_CALL(mock_compiler, call).WillOnce(Return(std::unexpected{compiler_error_t::invalid_authored}));

    auto const result = sut.apply(device, profile, attachment_id_t{1}, configuration::apply_mode_t::active);

    EXPECT_EQ(result.error(), (sut_t::apply_error_t{compiler_error_t::invalid_authored}));
}

TEST_F(session_test_t, control_apply_failure_is_preserved)
{
    auto const expected = error_t{.code = error_code_t::attachment_unavailable, .native_error = ENODEV};
    EXPECT_CALL(mock_compiler, call).WillOnce(Return(runtime));
    EXPECT_CALL(mock_client, apply).WillOnce(Return(std::unexpected{expected}));

    auto const result = sut.apply(device, profile, attachment_id_t{19}, configuration::apply_mode_t::active);

    EXPECT_EQ(result.error(), (sut_t::apply_error_t{expected}));
}

TEST_F(session_open_test_t, constructs_session_after_client_open_succeeds)
{
    auto const result = generic::session_t<open_compiler_t, successful_open_client_t>::open();

    EXPECT_TRUE(result.has_value());
}

TEST_F(session_open_test_t, preserves_client_open_failure)
{
    using sut_t = generic::session_t<open_compiler_t, failing_open_client_t>;

    auto const result = sut_t::open();

    EXPECT_EQ(result.error(), failing_open_client_t::open_error);
}

} // namespace
} // namespace crv::pipeline::control
