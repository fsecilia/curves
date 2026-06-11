// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/integer.hpp>
#include <crv/ui/command/command.hpp>
#include <QUndoCommand>
#include <chrono>
#include <utility>

namespace crv::command::qt {

/// adapts native command in a qt command
template <typename command_t> class command_adapter_t : public QUndoCommand
{
public:
    using idle_clock_t = idle_clock_t;
    using idle_time_point_t = idle_clock_t::time_point;

    explicit command_adapter_t(
        command_t command, idle_time_point_t idle_time_point = idle_clock_t::now(), QUndoCommand* parent = nullptr)
        : QUndoCommand{parent}, command_{std::move(command)}, idle_time_point_{idle_time_point}
    {}

    auto id() const -> int override { return int_cast<int>(static_cast<int_t>(command_.id())); }

    auto redo() -> void override { command_.apply(); }
    auto undo() -> void override { command_.undo(); }

    auto mergeWith(QUndoCommand const* other) -> bool override
    {
        if constexpr (is_mergeable_command<command_t>)
        {
            auto const* typed_other = dynamic_cast<command_adapter_t const*>(other);

            // bounce if commands are different types
            auto const same_command_type = typed_other != nullptr;
            if (!same_command_type) return false;

            // bounce if idle time exceeded
            auto const idle_duration = typed_other->idle_time_point_ - idle_time_point_;
            auto const idle_duration_exceeded = idle_duration > command_.idle_duration();
            if (idle_duration_exceeded) return false;

            // bounce if command rejects merge
            if (!command_.try_merge(typed_other->command_)) return false;

            // restart timer from later event
            idle_time_point_ = typed_other->idle_time_point_;

            return true;
        }
        else return false;
    }

private:
    command_t command_;
    idle_time_point_t idle_time_point_;
};

} // namespace crv::command::qt
