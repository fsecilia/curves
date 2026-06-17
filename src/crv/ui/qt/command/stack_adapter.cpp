// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "stack_adapter.hpp"

namespace crv::command::qt {

new_stack_adapter_t::new_stack_adapter_t(stack_t& stack) noexcept : stack_{&stack}
{}

auto new_stack_adapter_t::canUndo() const noexcept -> bool
{
    return stack_->can_undo();
}

auto new_stack_adapter_t::canRedo() const noexcept -> bool
{
    return stack_->can_redo();
}

void new_stack_adapter_t::undo()
{
    if (!canUndo()) return;
    stack_->undo();
}

void new_stack_adapter_t::redo()
{
    if (!canRedo()) return;
    stack_->redo();
}

void new_stack_adapter_t::clear() noexcept
{
    return stack_->clear();
}

auto new_stack_adapter_t::on_push() -> void
{
    check_conditions();
}

auto new_stack_adapter_t::on_undo() -> void
{
    Q_EMIT undoApplied();
    check_conditions();
}

auto new_stack_adapter_t::on_redo() -> void
{
    Q_EMIT redoApplied();
    check_conditions();
}

auto new_stack_adapter_t::on_clear() -> void
{
    check_conditions();
}

auto new_stack_adapter_t::check_conditions() -> void
{
    check_can_undo();
    check_can_redo();
}

auto new_stack_adapter_t::check_can_undo() -> void
{
    auto const new_can_undo = stack_->can_undo();
    if (prev_can_undo_ != new_can_undo)
    {
        prev_can_undo_ = new_can_undo;
        Q_EMIT canUndoChanged(new_can_undo);
    }
}

auto new_stack_adapter_t::check_can_redo() -> void
{
    auto const new_can_redo = stack_->can_redo();
    if (prev_can_redo_ != new_can_redo)
    {
        prev_can_redo_ = new_can_redo;
        Q_EMIT canRedoChanged(new_can_redo);
    }
}

} // namespace crv::command::qt
