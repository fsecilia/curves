// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <chrono>
#include <concepts>

namespace crv::command {

/// command pattern command interface
struct command_i
{
    using clock_t = std::chrono::steady_clock;
    using duration_t = clock_t::duration;
    using time_point_t = clock_t::time_point;

    virtual ~command_i();

    /// applies command
    virtual auto redo() -> void = 0;

    /// reverts command
    virtual auto undo() -> void = 0;

    /// \returns maximum duration to consider merging commands; 0 for nonmergeable
    virtual auto merge_timeout() const noexcept -> duration_t = 0;

    /// folds src into *this so one undo()/redo() spans both
    ///
    /// This method is invoked after src.redo() has applied. Clients must take care to only move out of src if the merge
    /// is guaranteed to succeed without throwing.
    ///
    /// \returns true if command was correct type, referred to the same target, and was successfully merged
    virtual auto try_merge(command_i&& src) -> bool = 0;
};

/// provides default implementations for commands that can never be merged
class nonmergeable_command_t : public command_i
{
public:
    auto merge_timeout() const noexcept -> duration_t override;
    auto try_merge(command_i&&) noexcept -> bool override;
};

/// provides strongly-typed version of try_merge() to derived types, saving the dynamic_cast prologue per
template <typename derived_t> class mergeable_command_t : public command_i
{
public:
    auto try_merge(command_i&& src) -> bool final
    {
        auto* typed_src = dynamic_cast<derived_t*>(&src);
        if (!typed_src) return false;
        return try_merge(std::move(*typed_src));
    }

protected:
    /// strongly-typed overload for derived types to implement
    virtual auto try_merge(derived_t&& src) -> bool = 0;
};

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
