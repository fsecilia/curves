// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

namespace crv {

struct command_i
{
    virtual ~command_i() = default;

    virtual auto apply() -> void = 0;
    virtual auto undo() -> void = 0;
};

} // namespace crv
