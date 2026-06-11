// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <utility>

namespace crv::command {

/// encapsulates command stack against a toolkit-specific adapterementation
///
/// This adapter allows us to code against a toolkit without depending on the toolkit from generic code.
template <typename adapter_t> class stack_t
{
public:
    constexpr stack_t(adapter_t adapter) noexcept : adapter_{std::move(adapter)} {}

    template <typename command_t, typename... args_t> auto emplace(bool mergeable, args_t&&... args) -> void
    {
        adapter_.template emplace<command_t>(mergeable, std::forward<args_t>(args)...);
    }

    constexpr auto undo() -> void { adapter_.undo(); }
    constexpr auto redo() -> void { adapter_.redo(); }

private:
    adapter_t adapter_;
};

} // namespace crv::command
