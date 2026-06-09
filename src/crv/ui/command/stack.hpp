// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <utility>

namespace crv::command {

/// adapts command stack to an implementation
///
/// This adapter allows us to code against qt without depending on qt.
template <typename impl_t> class stack_adapter_t
{
public:
    constexpr stack_adapter_t(impl_t impl) noexcept : impl_{std::move(impl)} {}

    template <typename command_t> auto push(command_t command) -> void { impl_.push(std::move(command)); }

    constexpr auto undo() -> void { impl_.undo(); }
    constexpr auto redo() -> void { impl_.redo(); }

private:
    impl_t impl_;
};

} // namespace crv::command
