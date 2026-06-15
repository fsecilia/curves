// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "timeline.hpp"
#include <crv/test/test.hpp>

namespace crv::command {
namespace {

// commit to full: {10, 20, ->}
constexpr auto commit_to_full() -> timeline_t<int_t, 2>
{
    auto timeline = timeline_t<int_t, 2>{};

    timeline.reserve();
    timeline.commit(10);

    timeline.reserve();
    timeline.commit(20);

    return timeline;
}
static_assert(20 == commit_to_full().present());
static_assert(commit_to_full().can_step_backward());
static_assert(!commit_to_full().can_step_forward());

// commit to full, then step backward: {10, ->20}
constexpr auto commit_to_full_then_step_backward_once() -> timeline_t<int_t, 2>
{
    auto timeline = commit_to_full();
    timeline.step_backward();
    return timeline;
}
static_assert(10 == commit_to_full_then_step_backward_once().present());
static_assert(commit_to_full_then_step_backward_once().can_step_backward());
static_assert(commit_to_full_then_step_backward_once().can_step_forward());
static_assert(20 == commit_to_full_then_step_backward_once().future());

// commit to full, then step backward all the way to begin: {->10, 20}
constexpr auto commit_to_full_then_step_backward_to_begin() -> timeline_t<int_t, 2>
{
    auto timeline = commit_to_full_then_step_backward_once();
    timeline.step_backward();
    return timeline;
}
static_assert(!commit_to_full_then_step_backward_to_begin().can_step_backward());
static_assert(commit_to_full_then_step_backward_to_begin().can_step_forward());
static_assert(10 == commit_to_full_then_step_backward_to_begin().future());

// commit to full, then step backward: {10, ->20}
// committing new value overwrites future: {10, 30, ->}
constexpr auto commit_to_full_then_step_backward_then_commit() -> timeline_t<int_t, 2>
{
    auto timeline = commit_to_full_then_step_backward_once();
    timeline.reserve();
    timeline.commit(30);
    return timeline;
}
static_assert(30 == commit_to_full_then_step_backward_then_commit().present());
static_assert(commit_to_full_then_step_backward_then_commit().can_step_backward());
static_assert(!commit_to_full_then_step_backward_then_commit().can_step_forward());

// start with {10, 30, ->}, then step backwqrd
constexpr auto commit_to_full_then_step_backward_then_commit_then_step_backward() -> timeline_t<int_t, 2>
{
    auto timeline = commit_to_full_then_step_backward_then_commit();
    timeline.step_backward();
    return timeline;
}
static_assert(10 == commit_to_full_then_step_backward_then_commit_then_step_backward().present());

// commit to full, clear: {}
constexpr auto commit_to_full_then_clear() -> timeline_t<int_t, 2>
{
    auto timeline = commit_to_full();
    timeline.clear();
    return timeline;
}
static_assert(!commit_to_full_then_clear().can_step_backward());
static_assert(!commit_to_full_then_clear().can_step_forward());

//
// death tests
//

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

struct command_timeline_death_test_t : Test
{
    timeline_t<int, 1> timeline;
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
