// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <utility>

namespace crv::command {

/// command pattern command for undo stack
struct command_i
{
    virtual ~command_i();

    virtual auto apply() -> void = 0;
    virtual auto undo() -> void = 0;
};

class factory_t
{
public:
    template <typename command_t, typename... args_t> constexpr auto create(args_t&&... args) const -> command_t
    {
        return command_t{std::forward<args_t>(args)...};
    }
};

} // namespace crv::command
