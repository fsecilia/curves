// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "command.hpp"

namespace crv::command {

command_i::~command_i() = default;

auto nonmergeable_command_t::merge_timeout() const noexcept -> duration_t
{
    return duration_t{0};
}

auto nonmergeable_command_t::try_merge(command_i&&) noexcept -> bool
{
    return false;
}

} // namespace crv::command
