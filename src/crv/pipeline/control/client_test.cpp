// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "client.hpp"
#include <crv/pipeline.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::pipeline::control {
namespace {

using namespace testing;

struct client_test_t : Test
{
    using io_result_t = std::expected<void, int_t>;

    struct mock_io_t
    {
        MOCK_METHOD(io_result_t, get_device, (crv_control_device_v1_t&), (const));
        MOCK_METHOD(io_result_t, apply, (crv_control_apply_v1_t const&), (const));
    };
    StrictMock<mock_io_t> mock_io;
    InSequence sequence;

    struct io_delegate_t
    {
        mock_io_t* mock;

        auto get_device(crv_control_device_v1_t& request) const -> io_result_t { return mock->get_device(request); }
        auto apply(crv_control_apply_v1_t const& request) const -> io_result_t { return mock->apply(request); }
    };

    using sut_t = generic::client_t<io_delegate_t>;

    struct successful_open_io_t
    {
        [[nodiscard]] static auto open() -> std::expected<successful_open_io_t, int_t> { return successful_open_io_t{}; }
    };

    struct failing_open_io_t
    {
        [[nodiscard]] static auto open() -> std::expected<failing_open_io_t, int_t>
        {
            return std::unexpected{int_t{EACCES}};
        }
    };

    struct captured_apply_t
    {
        crv_control_apply_v1_t request{};
        crv_control_configuration_v1_t configuration{};
    };

    static auto device(crv_u64_t id, crv_u16_t bustype = 3, crv_u16_t vendor = 4, crv_u16_t product = 5,
        crv_u16_t version = 6, char const* sysname = "input7") -> crv_control_device_v1_t
    {
        auto result = crv_control_device_v1_t{};
        result.attachment_id = id;
        result.bustype = bustype;
        result.vendor = vendor;
        result.product = product;
        result.version = version;
        std::strncpy(result.sysname, sysname, sizeof(result.sysname) - 1);
        return result;
    }

    auto return_device(crv_control_device_v1_t value) -> void
    {
        EXPECT_CALL(mock_io, get_device).WillOnce(Invoke([value](crv_control_device_v1_t& request) mutable {
            auto const after = request.after_attachment_id;
            request = value;
            request.after_attachment_id = after;
            return io_result_t{};
        }));
    }

    auto end_enumeration() -> void
    {
        EXPECT_CALL(mock_io, get_device).WillOnce(Return(std::unexpected{int_t{ENOENT}}));
    }

    auto capture_apply(io_result_t result = {}) -> void
    {
        EXPECT_CALL(mock_io, apply).WillOnce(Invoke([this, result](crv_control_apply_v1_t const& request) {
            captured_apply.request = request;
            auto const* configuration
                = reinterpret_cast<crv_control_configuration_v1_t const*>(request.configuration);
            captured_apply.configuration = *configuration;
            return result;
        }));
    }

    sut_t sut{{&mock_io}};
    captured_apply_t captured_apply;
    configuration::runtime_t runtime;
};

TEST_F(client_test_t, open_returns_client_after_io_open_succeeds)
{
    auto const result = generic::client_t<successful_open_io_t>::open();

    EXPECT_TRUE(result.has_value());
}

TEST_F(client_test_t, open_preserves_io_open_failure)
{
    auto const result = generic::client_t<failing_open_io_t>::open();

    EXPECT_EQ(result.error(), (error_t{.code = error_code_t::endpoint_open_failed, .native_error = EACCES}));
}

TEST_F(client_test_t, enumeration_returns_zero_devices_successfully)
{
    end_enumeration();

    auto const result = sut.devices();

    EXPECT_TRUE(result->empty());
}

TEST_F(client_test_t, enumeration_preserves_attachment_identity)
{
    return_device(device(41, 7, 11, 13, 17, "event23"));
    end_enumeration();

    auto const result = sut.devices();

    EXPECT_EQ(result->front(), (attachment_t{
                                   .id = attachment_id_t{41},
                                   .bustype = 7,
                                   .vendor = 11,
                                   .product = 13,
                                   .version = 17,
                                   .sysname = "event23",
                               }));
}

TEST_F(client_test_t, enumeration_walks_multiple_devices)
{
    return_device(device(2));
    return_device(device(9));
    return_device(device(31));
    end_enumeration();

    auto const result = sut.devices();

    EXPECT_EQ(*result, (std::vector<attachment_t>{
                           attachment_t{.id = attachment_id_t{2}, .bustype = 3, .vendor = 4, .product = 5,
                               .version = 6, .sysname = "input7"},
                           attachment_t{.id = attachment_id_t{9}, .bustype = 3, .vendor = 4, .product = 5,
                               .version = 6, .sysname = "input7"},
                           attachment_t{.id = attachment_id_t{31}, .bustype = 3, .vendor = 4, .product = 5,
                               .version = 6, .sysname = "input7"},
                       }));
}

TEST_F(client_test_t, enumeration_passes_previous_attachment_id_to_next_request)
{
    EXPECT_CALL(mock_io, get_device).WillOnce(Invoke([](crv_control_device_v1_t& request) {
        request.attachment_id = 37;
        return io_result_t{};
    }));
    EXPECT_CALL(mock_io, get_device).WillOnce(Invoke([](crv_control_device_v1_t& request) {
        if (request.after_attachment_id != 37) return io_result_t{std::unexpected{int_t{EINVAL}}};
        return io_result_t{std::unexpected{int_t{ENOENT}}};
    }));

    auto const result = sut.devices();

    EXPECT_TRUE(result.has_value());
}

TEST_F(client_test_t, enumeration_surfaces_first_call_failure)
{
    EXPECT_CALL(mock_io, get_device).WillOnce(Return(std::unexpected{int_t{EIO}}));

    auto const result = sut.devices();

    EXPECT_EQ(result.error(), (error_t{.code = error_code_t::enumeration_failed, .native_error = EIO}));
}

TEST_F(client_test_t, enumeration_surfaces_midstream_failure)
{
    return_device(device(2));
    EXPECT_CALL(mock_io, get_device).WillOnce(Return(std::unexpected{int_t{EIO}}));

    auto const result = sut.devices();

    EXPECT_EQ(result.error(), (error_t{.code = error_code_t::enumeration_failed, .native_error = EIO}));
}

TEST_F(client_test_t, enumeration_rejects_nonprogressing_attachment_id)
{
    return_device(device(0));

    auto const result = sut.devices();

    EXPECT_EQ(result.error(), (error_t{.code = error_code_t::enumeration_failed, .native_error = EPROTO}));
}

TEST_F(client_test_t, enumeration_rejects_unterminated_sysname)
{
    auto value = device(1);
    std::memset(value.sysname, 'x', sizeof(value.sysname));
    return_device(value);

    auto const result = sut.devices();

    EXPECT_EQ(result.error(), (error_t{.code = error_code_t::enumeration_failed, .native_error = EPROTO}));
}

TEST_F(client_test_t, apply_returns_success_after_successful_ioctl)
{
    capture_apply();

    auto const result = sut.apply(attachment_id_t{1}, runtime, configuration::apply_mode_t::active);

    EXPECT_TRUE(result.has_value());
}

TEST_F(client_test_t, apply_encodes_selected_attachment_id)
{
    capture_apply();

    auto const result = sut.apply(attachment_id_t{73}, runtime, configuration::apply_mode_t::active);
    static_cast<void>(result);

    EXPECT_EQ(captured_apply.request.attachment_id, 73u);
}

TEST_F(client_test_t, apply_encodes_active_mode)
{
    capture_apply();

    auto const result = sut.apply(attachment_id_t{1}, runtime, configuration::apply_mode_t::active);
    static_cast<void>(result);

    EXPECT_EQ(captured_apply.request.mode, CRV_CONTROL_APPLY_MODE_ACTIVE);
}

TEST_F(client_test_t, apply_encodes_bypassed_mode)
{
    capture_apply();

    auto const result = sut.apply(attachment_id_t{1}, runtime, configuration::apply_mode_t::bypassed);
    static_cast<void>(result);

    EXPECT_EQ(captured_apply.request.mode, CRV_CONTROL_APPLY_MODE_BYPASSED);
}

TEST_F(client_test_t, apply_passes_runtime_config_representation)
{
    runtime.config.velocity_scale = pipeline_t::velocity_scale_t::literal(0x123456789);
    runtime.config.half_life = pipeline_t::duration_t::literal(0x23456789);
    capture_apply();

    auto const result = sut.apply(attachment_id_t{1}, runtime, configuration::apply_mode_t::active);
    static_cast<void>(result);

    EXPECT_EQ(std::memcmp(&captured_apply.configuration.config, &runtime.config, sizeof(runtime.config)), 0);
}

TEST_F(client_test_t, apply_passes_runtime_gain_representation)
{
    using tangent_t = pipeline_t::gain_t::extended_tangent_t;
    runtime.gain.extend_final_tangent = tangent_t{
        .slope = {.mantissa = 0x12345, .shift = 17},
        .y0 = tangent_t::y_t::literal(0x23456),
        .x_max_delta = tangent_t::x_t::literal(0x34567),
    };
    capture_apply();

    auto const result = sut.apply(attachment_id_t{1}, runtime, configuration::apply_mode_t::active);
    static_cast<void>(result);

    EXPECT_EQ(std::memcmp(&captured_apply.configuration.gain, &runtime.gain, sizeof(captured_apply.configuration.gain)),
        0);
}

TEST_F(client_test_t, apply_encodes_configuration_size)
{
    capture_apply();

    auto const result = sut.apply(attachment_id_t{1}, runtime, configuration::apply_mode_t::active);
    static_cast<void>(result);

    EXPECT_EQ(captured_apply.request.configuration_size, sizeof(crv_control_configuration_v1_t));
}

TEST_F(client_test_t, apply_surfaces_missing_attachment)
{
    capture_apply(std::unexpected{int_t{ENODEV}});

    auto const result = sut.apply(attachment_id_t{1}, runtime, configuration::apply_mode_t::active);

    EXPECT_EQ(result.error(), (error_t{.code = error_code_t::attachment_unavailable, .native_error = ENODEV}));
}

TEST_F(client_test_t, apply_surfaces_kernel_rejection)
{
    capture_apply(std::unexpected{int_t{EINVAL}});

    auto const result = sut.apply(attachment_id_t{1}, runtime, configuration::apply_mode_t::active);

    EXPECT_EQ(result.error(), (error_t{.code = error_code_t::apply_rejected, .native_error = EINVAL}));
}

TEST_F(client_test_t, apply_preserves_other_ioctl_failure)
{
    capture_apply(std::unexpected{int_t{EIO}});

    auto const result = sut.apply(attachment_id_t{1}, runtime, configuration::apply_mode_t::active);

    EXPECT_EQ(result.error(), (error_t{.code = error_code_t::apply_failed, .native_error = EIO}));
}

} // namespace
} // namespace crv::pipeline::control
