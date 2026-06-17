// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/ui/command/command.hpp>
#include <functional>
#include <utility>

namespace crv::command {

/// command pattern command to mutate a parameter and notify on change
template <typename param_t> class mutate_param_t : public mergeable_command_t<mutate_param_t<param_t>>
{
public:
    using value_t = param_t::value_t;
    using notify_t = std::function<void(param_t& changed_param, value_t prev)>;
    using merge_timeout_t = command_i::duration_t;

    constexpr mutate_param_t(bool mergeable, param_t& param, value_t next, notify_t notify) noexcept
        : param_{&param}, prev_{param.value()}, next_{std::move(next)}, notify_{std::move(notify)},
          mergeable_{mergeable}
    {}

    constexpr auto redo() -> void override { change(next_); }
    constexpr auto undo() -> void override { change(prev_); }

    static constexpr auto merge_timeout_ms = std::chrono::milliseconds{500};
    constexpr auto merge_timeout() const noexcept -> merge_timeout_t override
    {
        return mergeable_ ? std::chrono::duration_cast<merge_timeout_t>(merge_timeout_ms) : merge_timeout_t{0};
    }

    constexpr auto try_merge(mutate_param_t&& other) noexcept -> bool override
    {
        auto const affect_same_address = param_ == other.param_;
        if (!affect_same_address) return false;
        next_ = std::move(other).next_;
        return true;
    }

private:
    constexpr auto change(value_t const& value) -> void
    {
        auto const cur = param_->value();
        param_->value(value);
        notify_(*param_, cur);
    }

    param_t* param_;
    value_t prev_;
    value_t next_;
    notify_t notify_;
    bool mergeable_;
};

} // namespace crv::command
