// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2026 Frank Secilia
*/

#include "refinement_queue.hpp"
#include <curves/testing/test.hpp>
#include <compare>
#include <gmock/gmock.h>

namespace curves::spline::segment {

namespace {

// ----------------------------------------------------------------------------
// Standard Tests
// ----------------------------------------------------------------------------

struct RefinementQueueTest : Test {
  using Sut = RefinementQueue<int_t>;
  Sut sut{};
};

TEST_F(RefinementQueueTest, DefaultInitializedEmpty) {
  EXPECT_TRUE(sut.empty());
}

TEST_F(RefinementQueueTest, EmptyAfterPrepare) {
  sut.prepare(2);
  EXPECT_TRUE(sut.empty());
}

TEST_F(RefinementQueueTest, NotEmptyAfterPush) {
  sut.prepare(2);
  sut.push(1);
  EXPECT_FALSE(sut.empty());
}

TEST_F(RefinementQueueTest, EmptyAfterPop) {
  sut.prepare(2);
  sut.push(1);
  EXPECT_EQ(1, sut.pop());
  EXPECT_TRUE(sut.empty());
}

TEST_F(RefinementQueueTest, PopsLargestError) {
  sut.prepare(2);
  sut.push(1);
  sut.push(10);
  EXPECT_EQ(10, sut.pop());
}

TEST_F(RefinementQueueTest, OrderCorrectAfteChurn) {
  sut.prepare(5);

  sut.push(1);
  sut.push(100);
  sut.push(10);

  EXPECT_EQ(100, sut.pop());

  sut.push(1000);

  EXPECT_EQ(1000, sut.pop());
  EXPECT_EQ(10, sut.pop());
  EXPECT_EQ(1, sut.pop());
}

TEST_F(RefinementQueueTest, PrepareClearsPrevious) {
  sut.prepare(5);

  sut.push(1);
  sut.push(100);
  sut.push(10);

  sut.prepare(2);
  EXPECT_TRUE(sut.empty());

  sut.push(3);
  sut.push(2);

  EXPECT_EQ(3, sut.pop());
  EXPECT_EQ(2, sut.pop());
  EXPECT_TRUE(sut.empty());
}

TEST_F(RefinementQueueTest, MultipleSameValues) {
  sut.prepare(3);
  sut.push(5);
  sut.push(2);
  sut.push(5);

  EXPECT_EQ(5, sut.pop());
  EXPECT_EQ(5, sut.pop());
  EXPECT_EQ(2, sut.pop());
}

// ----------------------------------------------------------------------------
// Death Tests
// ----------------------------------------------------------------------------

struct RefinementQueueDeathTest : RefinementQueueTest {
  //
};

TEST_F(RefinementQueueDeathTest, DefaultInitializedPop) {
  EXPECT_DEATH(sut.pop(), "pop on empty queue");
}

TEST_F(RefinementQueueDeathTest, DefaultInitializedPush) {
  EXPECT_DEATH(sut.push(0), "push on full queue");
}

TEST_F(RefinementQueueDeathTest, PushOnFull) {
  sut.prepare(2);
  sut.push(1);
  sut.push(10);
  EXPECT_DEATH(sut.push(100), "push on full queue");
}

TEST_F(RefinementQueueDeathTest, PopOnEmpty) {
  sut.prepare(2);
  sut.push(1);
  sut.push(10);
  EXPECT_EQ(10, sut.pop());
  EXPECT_EQ(1, sut.pop());
  EXPECT_DEATH(sut.pop(), "pop on empty queue");
}

// ----------------------------------------------------------------------------
// WorkItem-Specific Tests
// ----------------------------------------------------------------------------

// Order is not guaranteed, but multiple items are valid.
TEST(RefinementQueueTestWorkItems, UnstableOrderValid) {
  struct WorkItem {
    int_t error;
    int_t summand;

    auto operator<=>(const WorkItem& other) const noexcept -> auto {
      return error <=> other.error;
    }
    auto operator==(const WorkItem& other) const noexcept -> bool {
      return error == other.error;
    }
  };

  auto sut = curves::spline::segment::RefinementQueue<WorkItem>{};

  sut.prepare(3);

  sut.push(WorkItem{5, 3});
  sut.push(WorkItem{5, 11});
  sut.push(WorkItem{5, 7});

  auto sum_remaining = 3 + 7 + 11;
  sum_remaining -= sut.pop().summand;
  sum_remaining -= sut.pop().summand;
  sum_remaining -= sut.pop().summand;

  EXPECT_EQ(0, sum_remaining);
}

TEST(RefinementQueueTestWorkItems, SupportsMoveOnlyTypes) {
  struct MoveOnlyInt {
    int_t value;

    explicit MoveOnlyInt(int_t v) : value(v) {}

    MoveOnlyInt(MoveOnlyInt&&) = default;
    MoveOnlyInt& operator=(MoveOnlyInt&&) = default;
    MoveOnlyInt(const MoveOnlyInt&) = delete;
    MoveOnlyInt& operator=(const MoveOnlyInt&) = delete;
    auto operator<=>(const MoveOnlyInt&) const noexcept -> auto = default;
  };

  auto sut = RefinementQueue<MoveOnlyInt>{};
  sut.prepare(2);

  sut.push(MoveOnlyInt{10});
  sut.push(MoveOnlyInt{20});

  EXPECT_EQ(20, sut.pop().value);
  EXPECT_EQ(10, sut.pop().value);
}

TEST(RefinementQueueTestWorkItems, PrepareDestroysRemainingItems) {
  static auto destruction_count = 0;

  struct DtorTracker {
    ~DtorTracker() { destruction_count++; }
    auto operator<=>(const DtorTracker&) const noexcept -> auto = default;
  };

  auto sut = RefinementQueue<DtorTracker>{};
  sut.prepare(5);

  sut.push(DtorTracker{});
  sut.push(DtorTracker{});

  destruction_count = 0;
  sut.prepare(5);

  EXPECT_EQ(2, destruction_count);
}

}  // namespace
}  // namespace curves::spline::segment
