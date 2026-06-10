// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/integer.hpp>
#include <QUndoCommand>
#include <utility>

namespace crv::command::qt {

/// adapts native command in a qt command
template <typename command_t> class command_adapter_t : public QUndoCommand
{
public:
    explicit command_adapter_t(command_t command, QUndoCommand* parent = nullptr)
        : QUndoCommand{parent}, command_{std::move(command)}
    {}

    auto id() const -> int override { return int_cast<int>(static_cast<int_t>(command_.id())); }

    auto redo() -> void override { command_.apply(); }
    auto undo() -> void override { command_.undo(); }

private:
    command_t command_;
};

} // namespace crv::command::qt
