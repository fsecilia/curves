// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
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

    constexpr auto apply() -> void { change(next_); }
    constexpr auto undo() -> void { change(prev_); }

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
