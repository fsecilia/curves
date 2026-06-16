// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "timeline.hpp"
#include <crv/test/test.hpp>

namespace crv::command {
namespace {

struct move_only_t
{
    int_t value = 0;

    constexpr move_only_t() noexcept = default;
    constexpr move_only_t(int_t value) noexcept : value{value} {}

    constexpr move_only_t(move_only_t&&) noexcept = default;
    constexpr auto operator=(move_only_t&&) noexcept -> move_only_t& = default;

    move_only_t(move_only_t const&) = delete;
    auto operator=(move_only_t const&) -> move_only_t& = delete;
};

//
// type contract
//

// move_only_t must really be move-only, or the coverage in move_only_paths() is vacuous
static_assert(!std::is_copy_constructible_v<move_only_t>);
static_assert(std::is_nothrow_move_constructible_v<move_only_t>);

// timeline_t is move-only and nothrow-movable for any element type
static_assert(std::is_nothrow_move_constructible_v<timeline_t<int_t, 2>>);
static_assert(std::is_nothrow_move_assignable_v<timeline_t<int_t, 2>>);
static_assert(!std::is_copy_constructible_v<timeline_t<int_t, 2>>);
static_assert(!std::is_copy_assignable_v<timeline_t<int_t, 2>>);
static_assert(std::is_nothrow_move_constructible_v<timeline_t<move_only_t, 2>>);
static_assert(!std::is_copy_constructible_v<timeline_t<move_only_t, 2>>);

//
// runtime tests
//

struct command_timeline_test_t : Test
{
    constexpr auto step_forward(auto& timeline) -> void
    {
        ASSERT_TRUE(timeline.can_step_forward());
        timeline.step_forward();
    }

    constexpr auto step_backward(auto& timeline) -> void
    {
        ASSERT_TRUE(timeline.can_step_backward());
        timeline.step_backward();
    }
};

TEST_F(command_timeline_test_t, navigation_lifecycle)
{
    auto timeline = timeline_t<int_t, 2>{};

    // initial empty state
    EXPECT_FALSE(timeline.can_commit());
    EXPECT_FALSE(timeline.can_step_backward());
    EXPECT_FALSE(timeline.can_step_forward());

    // first commit
    timeline.reserve();
    EXPECT_TRUE(timeline.can_commit());
    auto& first_ref = timeline.commit(10);
    EXPECT_EQ(&first_ref, &timeline.present());
    EXPECT_EQ(first_ref, 10);
    EXPECT_EQ(timeline.present(), 10);
    EXPECT_TRUE(timeline.can_step_backward());
    EXPECT_FALSE(timeline.can_step_forward());

    // second commit
    timeline.reserve();
    EXPECT_TRUE(timeline.can_commit());
    auto& second_ref = timeline.commit(20);
    EXPECT_EQ(&second_ref, &timeline.present());
    EXPECT_EQ(second_ref, 20);

    // step backward
    step_backward(timeline);
    EXPECT_EQ(timeline.present(), 10);
    EXPECT_EQ(timeline.future(), 20);
    EXPECT_TRUE(timeline.can_step_forward());

    // step backward to origin
    step_backward(timeline);
    EXPECT_FALSE(timeline.can_step_backward());
    EXPECT_EQ(timeline.future(), 10);

    // step forward to end
    step_forward(timeline);
    step_forward(timeline);
    EXPECT_EQ(&second_ref, &timeline.present());
    EXPECT_EQ(second_ref, 20);
    EXPECT_FALSE(timeline.can_step_forward());
}

TEST_F(command_timeline_test_t, truncate_future)
{
    auto timeline = timeline_t<int_t, 4>{};

    // build history
    for (int const value : {10, 20, 30})
    {
        timeline.reserve();
        timeline.commit(value);
    }

    // rewind back to 10
    step_backward(timeline);
    step_backward(timeline);
    EXPECT_EQ(timeline.present(), 10);

    // commit new present, truncating
    timeline.reserve();
    timeline.commit(99);
    EXPECT_EQ(timeline.present(), 99);
    EXPECT_FALSE(timeline.can_step_forward());

    // only 10 and 99 exist
    step_backward(timeline);
    EXPECT_EQ(timeline.present(), 10);
    EXPECT_EQ(timeline.future(), 99);
}

TEST_F(command_timeline_test_t, capacity_growth_and_clearing)
{
    auto timeline = timeline_t<int_t, 8>{};

    EXPECT_EQ(timeline.capacity(), 0);

    // first reserve hits initial_capacity
    timeline.reserve();
    EXPECT_EQ(timeline.capacity(), 8);

    // reserve does nothing if space exists
    timeline.reserve();
    EXPECT_EQ(timeline.capacity(), 8);

    timeline.commit(10);

    // clearing drops events but retains capacity
    timeline.clear();
    EXPECT_EQ(timeline.capacity(), 8);
    EXPECT_FALSE(timeline.can_step_backward());
    EXPECT_FALSE(timeline.can_step_forward());
    EXPECT_TRUE(timeline.can_commit());
}

TEST_F(command_timeline_test_t, exhaustive_growth_rate)
{
    auto timeline = timeline_t<int_t, 1>{};

    timeline.reserve();
    EXPECT_EQ(timeline.capacity(), 1);
    timeline.commit(10);

    timeline.reserve();
    EXPECT_EQ(timeline.capacity(), 2);
    timeline.commit(20);

    timeline.reserve();
    EXPECT_EQ(timeline.capacity(), 4);
}

TEST_F(command_timeline_test_t, move_only_event_integrity)
{
    auto timeline = timeline_t<move_only_t, 1>{};

    // force allocations and moves
    timeline.reserve();
    timeline.commit(move_only_t{10});
    timeline.reserve();
    timeline.commit(move_only_t{20});
    timeline.reserve();
    timeline.commit(move_only_t{30});

    // truncate and move-insert
    step_backward(timeline);
    step_backward(timeline);
    timeline.reserve();
    timeline.commit(move_only_t{99});

    step_backward(timeline);
    EXPECT_EQ(timeline.present().value, 10);
    EXPECT_EQ(timeline.future().value, 99);
}

//
// death tests
//

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

struct command_timeline_death_test_t : command_timeline_test_t
{
    timeline_t<int_t, 1> timeline;
};

TEST_F(command_timeline_death_test_t, commit_without_capacity_asserts)
{
    timeline.reserve();
    timeline.commit(10);
    EXPECT_DEBUG_DEATH(timeline.commit(20);, "can_commit");
}

TEST_F(command_timeline_death_test_t, step_backward_beyond_start_asserts)
{
    EXPECT_DEBUG_DEATH(timeline.step_backward(), "can_step_backward");
}

TEST_F(command_timeline_death_test_t, step_forward_beyond_end_asserts)
{
    EXPECT_DEBUG_DEATH(timeline.step_forward(), "can_step_forward");
}

TEST_F(command_timeline_death_test_t, present_when_empty_asserts)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(timeline.present()), "can_step_backward");
}

TEST_F(command_timeline_death_test_t, present_at_begin_asserts)
{
    timeline.reserve();
    timeline.commit(10);
    step_backward(timeline);
    EXPECT_DEBUG_DEATH(static_cast<void>(timeline.present()), "can_step_backward");
}

TEST_F(command_timeline_death_test_t, future_when_empty_asserts)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(timeline.future()), "can_step_forward");
}

TEST_F(command_timeline_death_test_t, future_when_at_end_asserts)
{
    timeline.reserve();
    timeline.commit(10);
    EXPECT_DEBUG_DEATH(static_cast<void>(timeline.future()), "can_step_forward");
}

#endif

} // namespace
} // namespace crv::command
