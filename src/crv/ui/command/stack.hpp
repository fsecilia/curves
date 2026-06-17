// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/ui/command/command.hpp>
#include <crv/ui/command/timeline.hpp>
#include <memory>
#include <utility>

namespace crv::command {

/// encapsulates command stack against a toolkit-specific implementation
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

namespace detail {

template <typename command_t> struct timeline_event_t
{
    std::unique_ptr<command_t> command;
    command_t::time_point_t timestamp;
};

} // namespace detail

/// command pattern command stack
template <typename t_command_t = command_i, typename t_timeline_t = timeline_t<detail::timeline_event_t<t_command_t>>>
class new_stack_t
{
public:
    using command_t = t_command_t;
    using timeline_t = t_timeline_t;

    using event_t = timeline_t::event_t;

    using clock_t = command_t::clock_t;
    using duration_t = clock_t::duration;
    using time_point_t = clock_t::time_point;

    explicit constexpr new_stack_t(timeline_t timeline = {}) noexcept : timeline_{std::move(timeline)} {}

    constexpr new_stack_t(new_stack_t&&) noexcept = default;
    constexpr auto operator=(new_stack_t&&) noexcept -> new_stack_t& = default;

    /// executes command, appends it to end of stack, merges if possible, then cuts redo chain
    ///
    /// strong guarantee: This type reserves, executes, then commits. Reservation may throw std::bad_alloc. Command
    /// execution may throw, which propagates. This may record an observable change in capacity, but capacity is an
    /// implementation detail and not part of the type's logical state. The change in capacity does not compound. Commit
    /// can not throw.
    ///
    /// \pre command != nullptr
    /// \pre timestamp is nondecreasing across calls and causal
    /// \throws std::bad_alloc if reservation fails
    /// \throws ... exceptions from executing command propagate
    constexpr auto push(std::unique_ptr<command_t>&& command, time_point_t timestamp = clock_t::now()) -> void
    {
        assert(command != nullptr);
        assert(!timeline_.can_step_backward() || timestamp >= timeline_.present().timestamp);

        // blocks are ordered to preserve strong guarantee

        // reserve; it may throw std::bad_alloc
        timeline_.reserve();

        // execute command before committing; it may throw
        command->redo();

        // try merging
        auto merged = false;
        if (can_merge_ && timeline_.can_step_backward())
        {
            auto& present = timeline_.present();
            auto const try_merge = !timeout_expired(*present.command, present.timestamp, *command, timestamp);
            merged = try_merge && present.command->try_merge(std::move(*command));
            if (merged) present.timestamp = timestamp;
        }

        // commit
        if (!merged) timeline_.commit(event_t{std::move(command), timestamp});
        can_merge_ = true;
    }

    /// directly constructs and pushes command
    ///
    /// strong guarantee: This method is implemented in terms of push().
    ///
    /// \pre timestamp is nondecreasing across calls and causal
    /// \throws std::bad_alloc if reservation fails
    /// \throws ... exceptions from executing command propagate
    /// \sa push
    template <typename typed_command_t, typename... args_t>
    constexpr auto emplace(time_point_t timestamp, args_t&&... args) -> void
    {
        push(std::unique_ptr<command_t>{new typed_command_t{std::forward<args_t>(args)...}}, timestamp);
    }

    /// directly constructs and pushes command using current clock time
    ///
    /// strong guarantee: This method is implemented in terms of push().
    ///
    /// \throws std::bad_alloc if reservation fails
    /// \throws ... exceptions from executing command propagate
    /// \sa push
    template <typename command_t, typename... args_t> constexpr auto emplace_now(args_t&&... args) -> void
    {
        emplace<command_t>(clock_t::now(), std::forward<args_t>(args)...);
    }

    /// undos present command
    ///
    /// strong guarantee: If undoing command throws, stack is unchanged.
    ///
    /// \pre can_undo()
    /// \throws ... exceptions from undoing command propagate
    constexpr auto undo() -> void
    {
        assert(can_undo());

        // undo first; it may throw
        timeline_.present().command->undo();

        // commit
        timeline_.step_backward();
        can_merge_ = false;
    }

    /// redos immediate future command
    ///
    /// strong guarantee: If redoing command throws, stack is unchanged.
    ///
    /// \pre can_redo()
    /// \throws ... exceptions from redoing command propagate
    constexpr auto redo() -> void
    {
        assert(can_redo());

        // redo first; it may throw
        timeline_.future().command->redo();

        // commit
        timeline_.step_forward();
        can_merge_ = false;
    }

    [[nodiscard]] constexpr auto can_undo() const noexcept -> bool { return timeline_.can_step_backward(); }
    [[nodiscard]] constexpr auto can_redo() const noexcept -> bool { return timeline_.can_step_forward(); }

    constexpr auto clear() noexcept -> void
    {
        timeline_.clear();
        can_merge_ = false;
    }

private:
    // returns true if elapsed time is greater than either command's merge timeout
    constexpr auto timeout_expired(command_t const& present_command, time_point_t present_timestamp,
        command_t const& next_command, time_point_t next_timestamp) const noexcept -> bool
    {
        assert(next_timestamp >= present_timestamp);
        auto const elapsed = next_timestamp - present_timestamp;
        auto const timeout = std::min(present_command.merge_timeout(), next_command.merge_timeout());
        return elapsed < duration_t::zero() || timeout <= elapsed;
    }

    timeline_t timeline_;
    bool can_merge_ = false;
};

struct stack_observer_i
{
    virtual auto on_push() -> void = 0;
    virtual auto on_undo() -> void = 0;
    virtual auto on_redo() -> void = 0;
    virtual auto on_clear() -> void = 0;

protected:
    virtual ~stack_observer_i();
};

/// decorates a stack to make it observable
/// \sa stack_t
template <typename stack_t> class observable_stack_t
{
public:
    using command_t = stack_t::command_t;
    using clock_t = stack_t::clock_t;
    using time_point_t = stack_t::time_point_t;

    explicit constexpr observable_stack_t(stack_t stack = stack_t{}) noexcept : stack_{std::move(stack)} {}

    constexpr auto observer(this auto&& self) noexcept -> auto { return self.observer_; }
    constexpr auto observer(stack_observer_i* observer) noexcept -> void { observer_ = observer; }

    constexpr auto push(std::unique_ptr<command_t>&& command, time_point_t timestamp = clock_t::now()) -> void
    {
        assert(observer_);
        stack_.push(std::move(command), timestamp);
        observer_->on_push();
    }

    template <typename command_t, typename... args_t>
    constexpr auto emplace(time_point_t timestamp, args_t&&... args) -> void
    {
        assert(observer_);
        stack_.template emplace<command_t>(timestamp, std::forward<args_t>(args)...);
        observer_->on_push();
    }

    template <typename command_t, typename... args_t> constexpr auto emplace_now(args_t&&... args) -> void
    {
        assert(observer_);
        stack_.template emplace_now<command_t>(std::forward<args_t>(args)...);
        observer_->on_push();
    }

    constexpr auto undo() -> void
    {
        assert(observer_);
        stack_.undo();
        observer_->on_undo();
    }

    constexpr auto redo() -> void
    {
        assert(observer_);
        stack_.redo();
        observer_->on_redo();
    }

    [[nodiscard]] constexpr auto can_undo() const noexcept -> bool { return stack_.can_undo(); }
    [[nodiscard]] constexpr auto can_redo() const noexcept -> bool { return stack_.can_redo(); }

    constexpr auto clear() noexcept -> void
    {
        assert(observer_);
        stack_.clear();
        observer_->on_clear();
    }

private:
    stack_t stack_;
    stack_observer_i* observer_;
};

} // namespace crv::command
