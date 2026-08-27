// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/kernel/control/abi.h>
#include <crv/pipeline/configuration/apply_mode.hpp>
#include <cassert>
#include <cstddef>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

namespace crv::kernel::control {

struct apply_mode_decoder_t
{
    constexpr auto operator()(uint32_t raw) const noexcept -> std::optional<pipeline::configuration::apply_mode_t>
    {
        using mode_t = pipeline::configuration::apply_mode_t;

        switch (raw)
        {
            case CRV_CONTROL_APPLY_MODE_BYPASSED: return mode_t::bypassed;
            case CRV_CONTROL_APPLY_MODE_ACTIVE: return mode_t::active;
            default: return std::nullopt;
        }
    }
};

/// owns one transient configuration candidate through validation and commit
template <typename t_candidate_t, typename t_transaction_t> class prepared_configuration_t
{
public:
    using candidate_t = t_candidate_t;
    using transaction_t = t_transaction_t;
    using validation_result_t = typename transaction_t::validation_result_t;
    using validated_candidate_t = typename transaction_t::template validated_candidate_t<candidate_t>;

    constexpr explicit prepared_configuration_t(
        pipeline::configuration::apply_mode_t mode, transaction_t transaction = {}) noexcept
        : candidate_{.mode = mode}, transaction_{std::move(transaction)}
    {
        static_assert(std::is_trivially_copyable_v<decltype(candidate_.config)>);
        static_assert(std::is_trivially_copyable_v<decltype(candidate_.gain)>);
    }

    constexpr auto config_bytes() noexcept -> std::span<std::byte>
    {
        return std::as_writable_bytes(std::span{&candidate_.config, std::size_t{1}});
    }

    constexpr auto gain_bytes() noexcept -> std::span<std::byte>
    {
        return std::as_writable_bytes(std::span{&candidate_.gain, std::size_t{1}});
    }

    constexpr auto validate() noexcept -> validation_result_t
    {
        auto result = transaction_.validate(candidate_);
        if (!result)
        {
            validated_.reset();
            return result.error();
        }

        validated_.emplace(*result);
        return {};
    }

    constexpr auto validated() const noexcept -> bool { return validated_.has_value(); }

    template <typename target_t> constexpr auto commit(target_t& target) const noexcept -> void
    {
        assert(validated_.has_value() && "prepared_configuration_t: commit requires successful validation");
        transaction_.commit(target, *validated_);
    }

private:
    candidate_t candidate_{};
    [[no_unique_address]] transaction_t transaction_{};
    std::optional<validated_candidate_t> validated_{};
};

} // namespace crv::kernel::control
