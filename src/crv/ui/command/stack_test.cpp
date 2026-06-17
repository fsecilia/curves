// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "stack.hpp"
#include <crv/test/test.hpp>
#include <chrono>
#include <gmock/gmock.h>
#include <stdexcept>

namespace crv::command {
namespace {

//
// stack
//

struct command_stack_test_t : Test
{
    struct clock_t;
    using duration_t = std::chrono::nanoseconds;
    using time_point_t = std::chrono::time_point<clock_t, duration_t>;

    struct clock_t
    {
        using duration = duration_t;
        using time_point = time_point_t;

        static inline time_point current_time{};
        static auto now() noexcept -> time_point { return current_time; }
    };

    static constexpr auto merge_timeout = duration_t{10};
    struct command_i
    {
        using clock_t = clock_t;
        using duration_t = duration_t;
        using time_point_t = time_point_t;

        virtual ~command_i() = default;

        virtual auto redo() -> void = 0;
        virtual auto undo() -> void = 0;
        virtual auto merge_timeout() const noexcept -> duration_t = 0;
        virtual auto try_merge(command_i&& src) noexcept -> bool = 0;
    };

    struct mock_command_t : command_i
    {
        MOCK_METHOD(void, redo, (), (override));
        MOCK_METHOD(void, undo, (), (override));
        MOCK_METHOD(duration_t, merge_timeout, (), (const, noexcept, override));
        MOCK_METHOD(bool, try_merge, (command_i&&), (noexcept, override));
    };

    using timeline_event_t = detail::timeline_event_t<command_i>;
    using timeline_base_t = timeline_t<timeline_event_t>;
    struct timeline_t : timeline_base_t
    {
        bool* throw_from_reserve = nullptr;
        constexpr auto reserve() -> void
        {
            if (*throw_from_reserve) throw std::bad_alloc{};
            else return timeline_base_t::reserve();
        }
    };

    using sut_t = stack_t<command_i, timeline_t>;

    bool throw_from_reserve = false;
    sut_t sut{timeline_t{{}, &throw_from_reserve}};

    auto create_command() -> std::unique_ptr<StrictMock<mock_command_t>>
    {
        return std::make_unique<StrictMock<mock_command_t>>();
    }

    auto undo() -> void
    {
        ASSERT_TRUE(sut.can_undo());
        sut.undo();
    }

    auto redo() -> void
    {
        ASSERT_TRUE(sut.can_redo());
        sut.redo();
    }
};

TEST_F(command_stack_test_t, push_executes_and_commits_command)
{
    auto command = create_command();
    EXPECT_CALL(*command, redo());

    sut.push(std::move(command));

    EXPECT_TRUE(sut.can_undo());
    EXPECT_FALSE(sut.can_redo());
}

TEST_F(command_stack_test_t, push_provides_strong_guarantee_on_reserve_failure)
{
    auto command = create_command();

    // force throw from reserve
    throw_from_reserve = true;
    EXPECT_THROW(sut.push(std::move(command)), std::bad_alloc);

    // timeline remains in original state
    EXPECT_FALSE(sut.can_undo());
    EXPECT_FALSE(sut.can_redo());
}

TEST_F(command_stack_test_t, push_provides_strong_guarantee_on_redo_failure)
{
    auto command = create_command();

    // force throw from redo
    EXPECT_CALL(*command, redo()).WillOnce(Throw(std::runtime_error{"execution failed"}));

    EXPECT_THROW(sut.push(std::move(command)), std::runtime_error);

    // timeline remains in original state
    EXPECT_FALSE(sut.can_undo());
    EXPECT_FALSE(sut.can_redo());
}

TEST_F(command_stack_test_t, push_merges_commands_within_timeout)
{
    auto const base_time = time_point_t{duration_t{1000}};
    auto const merge_time = base_time + merge_timeout - duration_t{1};

    auto command_0 = create_command();
    auto& mock_0 = *command_0;
    auto command_1 = create_command();
    auto& mock_1 = *command_1;

    EXPECT_CALL(mock_0, redo());
    EXPECT_CALL(mock_0, merge_timeout()).WillRepeatedly(Return(merge_timeout));
    EXPECT_CALL(mock_0, try_merge(_)).WillOnce(Return(true));

    EXPECT_CALL(mock_1, redo());
    EXPECT_CALL(mock_1, merge_timeout()).WillRepeatedly(Return(merge_timeout));

    sut.push(std::move(command_0), base_time);
    sut.push(std::move(command_1), merge_time);

    // after merge, one undo clears both changes
    EXPECT_CALL(mock_0, undo());
    undo();

    EXPECT_FALSE(sut.can_undo());
}

TEST_F(command_stack_test_t, push_rejects_merge_when_timeout_expires)
{
    auto const base_time = time_point_t{duration_t{1000}};
    auto const expired_time = base_time + merge_timeout;

    auto command_0 = create_command();
    auto& mock_0 = *command_0;
    auto command_1 = create_command();
    auto& mock_1 = *command_1;

    EXPECT_CALL(mock_0, redo());
    EXPECT_CALL(mock_0, merge_timeout()).WillRepeatedly(Return(merge_timeout));
    // try_merge is not called here because timeout expired

    EXPECT_CALL(mock_1, redo());
    EXPECT_CALL(mock_1, merge_timeout()).WillRepeatedly(Return(merge_timeout));

    sut.push(std::move(command_0), base_time);
    sut.push(std::move(command_1), expired_time);

    // two undos available because they did not merge
    EXPECT_CALL(mock_1, undo());
    undo();
    EXPECT_TRUE(sut.can_undo());

    EXPECT_CALL(mock_0, undo());
    undo();
    EXPECT_FALSE(sut.can_undo());
}

// pushes 2 commands that merge, then pushes a 3rd that will only merge if timestamps are updated after merges
TEST_F(command_stack_test_t, push_updates_timestamp_on_successful_merge)
{
    auto const t0 = time_point_t{duration_t{0}};
    auto const t1 = time_point_t{duration_t{10}};
    auto const t2 = time_point_t{duration_t{25}};
    auto const timeout = duration_t{20};

    auto command_0 = create_command();
    auto& mock_0 = *command_0;
    auto command_1 = create_command();
    auto& mock_1 = *command_1;
    auto command_2 = create_command();
    auto& mock_2 = *command_2;

    EXPECT_CALL(mock_0, redo());
    EXPECT_CALL(mock_0, merge_timeout()).WillRepeatedly(Return(timeout));

    EXPECT_CALL(mock_1, redo());
    EXPECT_CALL(mock_1, merge_timeout()).WillRepeatedly(Return(timeout));

    EXPECT_CALL(mock_2, redo());
    EXPECT_CALL(mock_2, merge_timeout()).WillRepeatedly(Return(timeout));

    // push commands 0 and 1; if timestamp updates, present's new time is 10
    EXPECT_CALL(mock_0, try_merge(_)).WillOnce(Return(true));
    sut.push(std::move(command_0), t0);
    sut.push(std::move(command_1), t1);

    // push command 2 at t = 25; this is expired if present is still at t = 0, but if t = 10, elapsed is 15, which is
    // within the window of 20, so the merge proceeds
    EXPECT_CALL(mock_0, try_merge(_)).WillOnce(Return(true));
    sut.push(std::move(command_2), t2);

    // all three collapse into single event
    EXPECT_CALL(mock_0, undo());
    undo();
    EXPECT_FALSE(sut.can_undo());
}

TEST_F(command_stack_test_t, push_cuts_redo_chain)
{
    auto command_0 = create_command();
    auto& mock_0 = *command_0;
    auto command_1 = create_command();
    auto& mock_1 = *command_1;

    EXPECT_CALL(mock_0, redo());
    sut.push(std::move(command_0));

    EXPECT_CALL(mock_0, undo());
    undo();
    EXPECT_TRUE(sut.can_redo());

    // pushing command 1 while in past truncates future
    EXPECT_CALL(mock_1, redo());
    sut.push(std::move(command_1));

    EXPECT_FALSE(sut.can_redo());
}

TEST_F(command_stack_test_t, undo_and_redo_reapply_command_state)
{
    auto command = create_command();
    auto& mock = *command;

    EXPECT_CALL(mock, redo()).Times(2);
    EXPECT_CALL(mock, undo()).Times(1);

    sut.push(std::move(command));

    undo();
    EXPECT_FALSE(sut.can_undo());
    EXPECT_TRUE(sut.can_redo());

    redo();
    EXPECT_TRUE(sut.can_undo());
    EXPECT_FALSE(sut.can_redo());
}

TEST_F(command_stack_test_t, clear_wipes_timeline_and_mergeability)
{
    auto command = create_command();
    auto& mock = *command;

    EXPECT_CALL(mock, redo());
    sut.push(std::move(command));

    sut.clear();

    EXPECT_FALSE(sut.can_undo());
    EXPECT_FALSE(sut.can_redo());
}

//
// observable_stack_t
//

struct command_observable_stack_test_t : Test
{
    struct arg_0_t
    {};
    arg_0_t const arg_0{};

    struct arg_1_t
    {};
    arg_1_t const arg_1{};

    using time_point_t = int_t;
    static constexpr time_point_t timestamp = 17;
    struct clock_t
    {
        static auto now() noexcept -> time_point_t { return timestamp; }
    };

    struct command_t
    {
        using clock_t = clock_t;
        using time_point_t = time_point_t;

        int_t id = 0;

        constexpr auto operator==(command_t const&) const noexcept -> bool = default;
    };

    struct mock_stack_t
    {
        virtual ~mock_stack_t() = default;

        MOCK_METHOD(void, push, (std::unique_ptr<command_t>&&, time_point_t));
        MOCK_METHOD(void, emplace, (time_point_t timestamp, arg_0_t const& arg_0, arg_1_t const& arg1));
        MOCK_METHOD(void, emplace_now, (arg_0_t const& arg_0, arg_1_t const& arg1));
        MOCK_METHOD(void, undo, ());
        MOCK_METHOD(void, redo, ());
        MOCK_METHOD(bool, can_undo, (), (const, noexcept));
        MOCK_METHOD(bool, can_redo, (), (const, noexcept));
        MOCK_METHOD(void, clear, ());
    };
    StrictMock<mock_stack_t> mock_stack;

    struct stack_t
    {
        using command_t = command_t;
        using clock_t = clock_t;
        using time_point_t = time_point_t;

        mock_stack_t* mock = nullptr;

        constexpr auto push(std::unique_ptr<command_t>&& command, time_point_t timestamp = clock_t::now()) -> void
        {
            mock->push(std::move(command), timestamp);
        }

        template <typename command_t, typename... args_t>
        constexpr auto emplace(time_point_t timestamp, args_t&&... args) -> void
        {
            mock->emplace(timestamp, std::forward<args_t>(args)...);
        }

        template <typename command_t, typename... args_t> constexpr auto emplace_now(args_t&&... args) -> void
        {
            mock->emplace_now(std::forward<args_t>(args)...);
        }

        constexpr auto undo() -> void { mock->undo(); }
        constexpr auto redo() -> void { mock->redo(); }
        [[nodiscard]] constexpr auto can_undo() const noexcept -> bool { return mock->can_undo(); }
        [[nodiscard]] constexpr auto can_redo() const noexcept -> bool { return mock->can_redo(); }

        constexpr auto clear() noexcept -> void { mock->clear(); }
    };

    struct mock_observer_t : stack_observer_i
    {
        MOCK_METHOD(void, on_push, ());
        MOCK_METHOD(void, on_undo, ());
        MOCK_METHOD(void, on_redo, ());
        MOCK_METHOD(void, on_clear, ());
    };
    StrictMock<mock_observer_t> mock_observer;

    using sut_t = observable_stack_t<stack_t>;
    sut_t sut{stack_t{&mock_stack}};

    command_observable_stack_test_t() noexcept { sut.observer(&mock_observer); }
};

TEST_F(command_observable_stack_test_t, push)
{
    std::unique_ptr<command_t> command = std::make_unique<command_t>();
    command_t& command_ref = *command;

    EXPECT_CALL(mock_stack, push(_, timestamp))
        .WillOnce(WithArg<0>(
            [&](std::unique_ptr<command_t>&& command) noexcept -> void { EXPECT_EQ(&command_ref, command.get()); }));
    EXPECT_CALL(mock_observer, on_push());

    sut.push(std::move(command), timestamp);
}

TEST_F(command_observable_stack_test_t, emplace)
{
    EXPECT_CALL(mock_stack, emplace(timestamp, Ref(arg_0), Ref(arg_1)));
    EXPECT_CALL(mock_observer, on_push());

    sut.template emplace<command_t>(timestamp, arg_0, arg_1);
}

TEST_F(command_observable_stack_test_t, emplace_now)
{
    EXPECT_CALL(mock_stack, emplace_now(Ref(arg_0), Ref(arg_1)));
    EXPECT_CALL(mock_observer, on_push());

    sut.template emplace_now<command_t>(arg_0, arg_1);
}

TEST_F(command_observable_stack_test_t, undo)
{
    EXPECT_CALL(mock_stack, undo());
    EXPECT_CALL(mock_observer, on_undo());

    sut.undo();
}

TEST_F(command_observable_stack_test_t, redo)
{
    EXPECT_CALL(mock_stack, redo());
    EXPECT_CALL(mock_observer, on_redo());

    sut.redo();
}

TEST_F(command_observable_stack_test_t, can_undo_false)
{
    EXPECT_CALL(mock_stack, can_undo()).WillOnce(Return(false));
    EXPECT_FALSE(sut.can_undo());
}

TEST_F(command_observable_stack_test_t, can_undo_true)
{
    EXPECT_CALL(mock_stack, can_undo()).WillOnce(Return(true));
    EXPECT_TRUE(sut.can_undo());
}

TEST_F(command_observable_stack_test_t, can_redo_false)
{
    EXPECT_CALL(mock_stack, can_redo()).WillOnce(Return(false));
    EXPECT_FALSE(sut.can_redo());
}

TEST_F(command_observable_stack_test_t, can_redo_true)
{
    EXPECT_CALL(mock_stack, can_redo()).WillOnce(Return(true));
    EXPECT_TRUE(sut.can_redo());
}

TEST_F(command_observable_stack_test_t, clear)
{
    EXPECT_CALL(mock_stack, clear());
    EXPECT_CALL(mock_observer, on_clear());

    sut.clear();
}

} // namespace
} // namespace crv::command
