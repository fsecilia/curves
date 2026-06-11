// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <chrono>
#include <concepts>

namespace crv::command {

enum class id_t : int_t
{
    mutate_param,
};

using idle_clock_t = std::chrono::steady_clock;
using idle_duration_t = idle_clock_t::duration;

template <typename command_t>
concept is_mergeable_command = requires(command_t command, command_t const& other) {
    { command.idle_duration() } -> std::convertible_to<idle_duration_t>;
    { command.try_merge(other) } -> std::convertible_to<bool>;
};

} // namespace crv::command
