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
template <typename param_t> class mutate_param_t
{
public:
    using value_t = param_t::value_t;
    using notify_t = std::function<void(param_t& changed_param, value_t prev)>;

    constexpr mutate_param_t(param_t& param, value_t next, notify_t notify) noexcept
        : param_{&param}, prev_{param.value()}, next_{std::move(next)}, notify_{std::move(notify)}
    {}

    constexpr auto id() const noexcept -> id_t { return id_t::mutate_param; }
    constexpr auto apply() -> void { change(next_); }
    constexpr auto undo() -> void { change(prev_); }

    static constexpr auto idle_duration_ms = std::chrono::milliseconds{500};
    constexpr auto idle_duration() const noexcept -> idle_duration_t
    {
        return std::chrono::duration_cast<idle_duration_t>(idle_duration_ms);
    }

    constexpr auto try_merge(mutate_param_t const& other) noexcept -> bool
    {
        auto const affect_same_address = param_ == other.param_;
        if (!affect_same_address) return false;
        next_ = other.next_;
        return true;
    }

private:
    param_t* param_;
    value_t prev_;
    value_t next_;
    notify_t notify_;

    constexpr auto change(value_t const& value) -> void
    {
        auto const cur = param_->value();
        param_->value(value);
        notify_(*param_, cur);
    }
};

} // namespace crv::command
