// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2026 Frank Secilia
*/

#include "successor_map.hpp"
#include <curves/testing/test.hpp>
#include <gmock/gmock.h>

namespace curves {
namespace {

// ----------------------------------------------------------------------------
// Common Fixture
// ----------------------------------------------------------------------------

struct SegmentListTest : Test {
  struct Segment {
    int value;
    auto operator<=>(const Segment&) const = default;
  };

  using Sut = SegmentList<Segment>;
  Sut list = Sut(10);
};

TEST_F(SegmentListTest, BeginEndEqualInEmptyList) {
  EXPECT_EQ(list.begin(), list.end());
}

TEST_F(SegmentListTest, ConstIteratorsEqualMutableIterators) {
  list.push_back({10});

  const auto mutable_begin = list.begin();
  const auto const_begin = static_cast<const Sut&>(list).begin();
  const auto mutable_end = list.end();
  const auto const_end = static_cast<const Sut&>(list).end();

  EXPECT_EQ(mutable_begin.list, const_begin.list);
  EXPECT_EQ(mutable_begin.current, const_begin.current);
  EXPECT_EQ(mutable_end.list, const_end.list);
  EXPECT_EQ(mutable_end.current, const_end.current);
}

TEST_F(SegmentListTest, PushBackMaintainsOrder) {
  const auto id1 = list.push_back({10});
  const auto id2 = list.push_back({20});
  const auto id3 = list.push_back({30});

  // Head and tail matches, size is correct.
  EXPECT_EQ(list.head(), id1);
  EXPECT_EQ(list.tail(), id3);
  EXPECT_EQ(list.size(), 3);

  // Topology is correct.
  EXPECT_EQ(list.next(id1), id2);
  EXPECT_EQ(list.next(id2), id3);
  EXPECT_EQ(list.next(id3), NodeId::Null);

  // Data is correct.
  EXPECT_EQ(list[id1].value, 10);
  EXPECT_EQ(list[id2].value, 20);
  EXPECT_EQ(list[id3].value, 30);
}

TEST_F(SegmentListTest, InsertAfterSplicesMiddle) {
  const auto id_a = list.push_back({1});  // A
  const auto id_b = list.push_back({3});  // B

  // Insert '2' after '1'.
  const auto id_c = list.insert_after(id_a, {2});

  // Topology is correct: A -> C -> B
  EXPECT_EQ(list.next(id_a), id_c);
  EXPECT_EQ(list.next(id_c), id_b);
  EXPECT_EQ(list.next(id_b), NodeId::Null);

  // Tail is still B.
  EXPECT_EQ(list.tail(), id_b);
}

TEST_F(SegmentListTest, InsertAfterTailUpdatesTailPointer) {
  const auto id1 = list.push_back({10});

  // Insert after current tail.
  const auto id2 = list.insert_after(id1, {20});

  // Topology is correct.
  EXPECT_EQ(list.next(id1), id2);
  EXPECT_EQ(list.tail(), id2);
  EXPECT_EQ(list.next(id2), NodeId::Null);

  // Push_back attaches to new tail.
  const auto id3 = list.push_back({30});
  EXPECT_EQ(list.next(id2), id3);
  EXPECT_EQ(list.tail(), id3);
}

TEST_F(SegmentListTest, EnforcesCapacity) {
  // Reduce capacity drastically.
  list = Sut(2);

  list.push_back({1});
  list.push_back({2});

  // Once full, pushing fails fail.
  const auto id_fail = list.push_back({3});
  EXPECT_EQ(id_fail, NodeId::Null);
  EXPECT_EQ(list.size(), 2);

  // Reset clears everything.
  list.reset(5);
  EXPECT_TRUE(list.empty());
  EXPECT_EQ(list.head(), NodeId::Null);
  EXPECT_EQ(list.tail(), NodeId::Null);

  // Pushing works again.
  EXPECT_NE(list.push_back({1}), NodeId::Null);
}

TEST_F(SegmentListTest, IterateMutableList) {
  list.push_back({10});
  list.push_back({20});
  list.push_back({30});

  EXPECT_THAT(list,
              testing::ElementsAre(Segment{10}, Segment{20}, Segment{30}));
}

TEST_F(SegmentListTest, IterateConstList) {
  list.push_back({10});
  list.push_back({20});
  list.push_back({30});

  EXPECT_THAT(static_cast<const Sut&>(list),
              testing::ElementsAre(Segment{10}, Segment{20}, Segment{30}));
}

TEST_F(SegmentListTest, IteratorFollowsLogicalOrderNotPhysicalOrder) {
  // Start with physical [A, B] and logical A -> B.
  const auto id_a = list.push_back({10});
  list.push_back({30});

  // Insert C between A and B.
  // Physical: [A, B, C]
  // Logical:  A -> C -> B
  list.insert_after(id_a, {20});

  // Collect via iterator.
  auto values = std::vector<int>{};
  for (const auto& item : list) values.push_back(item.value);

  // Verify logical order (10, 20, 30), not physical order (10, 30, 20).
  EXPECT_THAT(values, testing::ElementsAre(10, 20, 30));
}

}  // namespace
}  // namespace curves
