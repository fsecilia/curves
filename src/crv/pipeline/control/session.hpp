// SPDX-License-Identifier: MIT

/// \file
/// \brief authored configuration application to live kernel attachments
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/model/config.hpp>
#include <crv/pipeline/configuration/apply_mode.hpp>
#include <crv/pipeline/configuration/compiler.hpp>
#include <crv/pipeline/control/client.hpp>
#include <expected>
#include <utility>
#include <variant>

namespace crv::pipeline::control {
namespace generic {

template <typename t_compiler_t, typename t_client_t> class session_t
{
public:
    using compiler_t = t_compiler_t;
    using client_t = t_client_t;
    using compiler_error_t = typename compiler_t::error_t;
    using control_error_t = typename client_t::apply_result_t::error_type;
    using devices_result_t = typename client_t::devices_result_t;
    using apply_error_t = std::variant<compiler_error_t, control_error_t>;
    using apply_result_t = std::expected<void, apply_error_t>;
    using open_result_t = std::expected<session_t, typename client_t::open_result_t::error_type>;

    session_t(compiler_t compiler, client_t client) noexcept
        : compiler_{std::move(compiler)}, client_{std::move(client)}
    {}

    [[nodiscard]] static auto open() -> open_result_t
    {
        auto client = client_t::open();
        if (!client) return std::unexpected{std::move(client.error())};

        return session_t{compiler_t{}, std::move(*client)};
    }

    auto devices() const -> devices_result_t { return client_.devices(); }

    auto apply(model::device_t const& device, model::profile_t const& profile, attachment_id_t attachment,
        configuration::apply_mode_t mode) const -> apply_result_t
    {
        auto runtime = compiler_(device, profile);
        if (!runtime)
        {
            return std::unexpected{apply_error_t{std::in_place_type<compiler_error_t>, std::move(runtime.error())}};
        }

        auto result = client_.apply(attachment, *runtime, mode);
        if (!result)
        {
            return std::unexpected{apply_error_t{std::in_place_type<control_error_t>, std::move(result.error())}};
        }

        return {};
    }

private:
    [[no_unique_address]] compiler_t compiler_;
    [[no_unique_address]] client_t client_;
};

} // namespace generic

using session_t = generic::session_t<configuration::compiler_t, client_t>;

} // namespace crv::pipeline::control
