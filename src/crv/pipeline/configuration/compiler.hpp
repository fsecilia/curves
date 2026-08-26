// SPDX-License-Identifier: MIT

/// \file
/// \brief authored model to validated runtime configuration
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/model/config.hpp>
#include <crv/pipeline/configuration/runtime.hpp>
#include <expected>
#include <utility>
#include <variant>

namespace crv::pipeline::configuration {

template <typename t_authored_validator_t, typename t_config_builder_t, typename t_gain_compiler_t,
    typename t_runtime_validator_t>
struct compiler_t
{
    using authored_validator_t = t_authored_validator_t;
    using config_builder_t = t_config_builder_t;
    using gain_compiler_t = t_gain_compiler_t;
    using runtime_validator_t = t_runtime_validator_t;

    using error_t = std::variant<typename authored_validator_t::result_t, typename gain_compiler_t::error_t,
        typename runtime_validator_t::result_t>;
    using result_t = std::expected<runtime_t, error_t>;

    [[no_unique_address]] authored_validator_t validate_authored;
    [[no_unique_address]] config_builder_t build_config;
    [[no_unique_address]] gain_compiler_t compile_gain;
    [[no_unique_address]] runtime_validator_t validate_runtime;

    auto operator()(model::device_t const& device, model::profile_t const& profile) const -> result_t
    {
        auto const authored_result = validate_authored(device, profile);
        if (!authored_result) return std::unexpected{error_t{authored_result}};

        auto runtime = runtime_t{.config = build_config(device, profile)};
        auto gain_result = compile_gain(runtime.gain, profile.curves);
        if (!gain_result) return std::unexpected{error_t{std::move(gain_result.error())}};

        auto const runtime_result = validate_runtime(runtime.config, runtime.gain);
        if (!runtime_result) return std::unexpected{error_t{runtime_result}};

        return runtime;
    }
};

} // namespace crv::pipeline::configuration
