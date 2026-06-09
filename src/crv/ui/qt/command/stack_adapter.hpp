// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/ui/qt/command/command_adapter.hpp>
#include <utility>

namespace crv::command::qt {

/// implements command stack over QUndoStack
template <typename impl_t> class stack_adapter_t
{
public:
    explicit stack_adapter_t(impl_t& impl) : impl_{&impl} {}

    template <typename command_t, typename... args_t> auto emplace(args_t&&... args) -> void
    {
        auto command = std::make_unique<command_adapter_t<command_t>>(command_t{std::forward<args_t>(args)...});
        impl_->push(command.get());
        command.release();
    }

    constexpr auto undo() -> void { impl_->undo(); }
    constexpr auto redo() -> void { impl_->redo(); }

private:
    impl_t* impl_;
};

} // namespace crv::command::qt
