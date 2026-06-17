// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/ui/command/stack.hpp>
#include <crv/ui/qt/command/command_adapter.hpp>
#include <utility>

namespace crv::command::qt {

/// implements command stack over QUndoStack
template <typename impl_t> class stack_adapter_t
{
public:
    explicit stack_adapter_t(impl_t& impl) : impl_{&impl} {}

    template <typename command_t, typename... args_t> auto emplace(bool mergeable, args_t&&... args) -> void
    {
        auto command
            = std::make_unique<command_adapter_t<command_t>>(command_t{std::forward<args_t>(args)...}, mergeable);
        impl_->push(command.get());
        command.release();
    }

    constexpr auto undo() -> void { impl_->undo(); }
    constexpr auto redo() -> void { impl_->redo(); }

private:
    impl_t* impl_;
};

/// adapts new_stack_t to QObject so it can be referenced from qml
class new_stack_adapter_t : public QObject, public stack_observer_i
{
    Q_OBJECT
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY canUndoChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY canRedoChanged)

public:
    using stack_t = observable_stack_t<new_stack_t<>>;

    explicit new_stack_adapter_t(stack_t& stack) noexcept;

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
