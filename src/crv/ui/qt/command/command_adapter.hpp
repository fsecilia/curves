// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
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

    void redo() override { command_.apply(); }
    void undo() override { command_.undo(); }

private:
    command_t command_;
};

} // namespace crv::command::qt
