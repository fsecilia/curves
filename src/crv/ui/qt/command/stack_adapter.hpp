// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/ui/command/stack.hpp>
#include <QObject>

namespace crv::command::qt {

/// adapts stack_t to QObject so it can be referenced from qml
class stack_adapter_t : public QObject, public stack_observer_i
{
    Q_OBJECT
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY canUndoChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY canRedoChanged)

public:
    using stack_t = observable_stack_t<stack_t<>>;

    explicit stack_adapter_t(stack_t& stack) noexcept;

    auto canUndo() const noexcept -> bool;
    auto canRedo() const noexcept -> bool;

public Q_SLOTS:
    void undo();
    void redo();
    void clear() noexcept;

Q_SIGNALS:
    void undoApplied();
    void redoApplied();
    void canUndoChanged(bool canUndo);
    void canRedoChanged(bool canRedo);

public:
    // observable_stack_t handlers
    auto on_push() -> void override;
    auto on_undo() -> void override;
    auto on_redo() -> void override;
    auto on_clear() -> void override;

private:
    auto check_conditions() -> void;
    auto check_can_undo() -> void;
    auto check_can_redo() -> void;

    stack_t* stack_ = nullptr;
    bool prev_can_undo_ = false;
    bool prev_can_redo_ = false;
};

} // namespace crv::command::qt
