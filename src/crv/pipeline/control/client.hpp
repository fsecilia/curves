// SPDX-License-Identifier: MIT

/// \file
/// \brief userspace kernel-control client
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/kernel/control/abi.h>
#include <crv/lib.hpp>
#include <crv/pipeline/configuration/apply_mode.hpp>
#include <crv/pipeline/configuration/runtime.hpp>
#include <crv/pipeline/control/linux_io.hpp>
#include <cerrno>
#include <compare>
#include <cstddef>
#include <cstring>
#include <expected>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace crv::pipeline::control {

static_assert(static_cast<crv_u32_t>(configuration::apply_mode_t::bypassed) == CRV_CONTROL_APPLY_MODE_BYPASSED);
static_assert(static_cast<crv_u32_t>(configuration::apply_mode_t::active) == CRV_CONTROL_APPLY_MODE_ACTIVE);

struct attachment_id_t
{
    uint64_t value{};

    auto operator<=>(attachment_id_t const&) const noexcept -> auto = default;
    auto operator==(attachment_id_t const&) const noexcept -> bool = default;
};

struct attachment_t
{
    attachment_id_t id{};
    uint16_t bustype{};
    uint16_t vendor{};
    uint16_t product{};
    uint16_t version{};
    std::string sysname;

    auto operator==(attachment_t const&) const noexcept -> bool = default;
};

enum class error_code_t : uint8_t
{
    endpoint_open_failed,
    enumeration_failed,
    attachment_unavailable,
    apply_rejected,
    apply_failed,
};

struct error_t
{
    error_code_t code{};
    int_t native_error{};

    auto operator==(error_t const&) const noexcept -> bool = default;
};

namespace generic {

template <typename io_t> class client_t
{
public:
    using devices_result_t = std::expected<std::vector<attachment_t>, error_t>;
    using apply_result_t = std::expected<void, error_t>;
    using open_result_t = std::expected<client_t, error_t>;

    explicit client_t(io_t io) noexcept : io_{std::move(io)} {}

    [[nodiscard]] static auto open() -> open_result_t
    {
        auto io = io_t::open();
        if (!io)
        {
            return std::unexpected{
                error_t{.code = error_code_t::endpoint_open_failed, .native_error = io.error()}};
        }

        return client_t{std::move(*io)};
    }

    auto devices() const -> devices_result_t
    {
        auto attachments = std::vector<attachment_t>{};
        auto after = uint64_t{};

        for (;;)
        {
            auto request = crv_control_device_v1_t{};
            request.after_attachment_id = after;
            auto const result = io_.get_device(request);
            if (!result)
            {
                if (result.error() == ENOENT) return attachments;
                return std::unexpected{error_t{.code = error_code_t::enumeration_failed,
                    .native_error = result.error()}};
            }

            auto attachment = decode_attachment(request, after);
            if (!attachment)
            {
                return std::unexpected{error_t{
                    .code = error_code_t::enumeration_failed, .native_error = attachment.error()}};
            }

            attachments.push_back(std::move(*attachment));
            after = request.attachment_id;
        }
    }

    auto apply(attachment_id_t attachment, configuration::runtime_t const& runtime,
        configuration::apply_mode_t mode) const -> apply_result_t
    {
        auto const configuration = encode_configuration(runtime);
        auto const request = crv_control_apply_v1_t{
            .attachment_id = attachment.value,
            .configuration = static_cast<crv_u64_t>(reinterpret_cast<uint_t>(&configuration)),
            .configuration_size = static_cast<crv_u32_t>(sizeof(configuration)),
            .mode = static_cast<crv_u32_t>(mode),
            .reserved = 0,
        };

        auto const result = io_.apply(request);
        if (result) return {};

        auto const code = result.error() == ENODEV   ? error_code_t::attachment_unavailable
            : result.error() == EINVAL               ? error_code_t::apply_rejected
                                                     : error_code_t::apply_failed;
        return std::unexpected{error_t{.code = code, .native_error = result.error()}};
    }

private:
    using runtime_config_t = decltype(configuration::runtime_t::config);
    using gain_t = decltype(configuration::runtime_t::gain);

    static_assert(sizeof(runtime_config_t) == sizeof(crv_control_runtime_config_v1_t));
    static_assert(sizeof(gain_t) >= sizeof(crv_control_gain_v1_t));
    static_assert(std::is_trivially_copyable_v<runtime_config_t>);
    static_assert(std::is_trivially_copyable_v<gain_t>);
    static_assert(sizeof(uint_t) <= sizeof(crv_u64_t));
    static_assert(sizeof(crv_control_configuration_v1_t) <= ~crv_u32_t{});

    static auto decode_attachment(crv_control_device_v1_t const& source, uint64_t after)
        -> std::expected<attachment_t, int_t>
    {
        if (source.attachment_id <= after) return std::unexpected{int_t{EPROTO}};

        auto length = std::size_t{};
        while (length < sizeof(source.sysname) && source.sysname[length] != '\0') ++length;
        if (length == sizeof(source.sysname)) return std::unexpected{int_t{EPROTO}};

        return attachment_t{
            .id = attachment_id_t{source.attachment_id},
            .bustype = source.bustype,
            .vendor = source.vendor,
            .product = source.product,
            .version = source.version,
            .sysname = std::string{source.sysname, length},
        };
    }

    static auto encode_configuration(configuration::runtime_t const& runtime) -> crv_control_configuration_v1_t
    {
        auto result = crv_control_configuration_v1_t{};
        std::memcpy(&result.config, &runtime.config, sizeof(result.config));
        std::memcpy(&result.gain, &runtime.gain, sizeof(result.gain));
        return result;
    }

    [[no_unique_address]] io_t io_;
};

} // namespace generic

using client_t = generic::client_t<linux_io_t>;

} // namespace crv::pipeline::control
