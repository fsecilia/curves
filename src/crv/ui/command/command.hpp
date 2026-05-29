// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

namespace crv {

/// command pattern command for undo stack
struct command_i
{
    virtual ~command_i();

    virtual auto apply() -> void = 0;
    virtual auto undo() -> void = 0;
};

} // namespace crv
