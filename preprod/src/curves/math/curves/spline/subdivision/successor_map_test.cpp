// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2026 Frank Secilia
*/

#include "successor_map.hpp"
#include <curves/testing/test.hpp>
#include <gmock/gmock.h>

namespace curves::spline::segment {
namespace {

// ----------------------------------------------------------------------------
// Standard Tests
// ----------------------------------------------------------------------------

struct SuccessorMapTest : Test {
  static constexpr auto capacity = std::size_t{5};

  using Sut = SuccessorMap;
  Sut sut{};
};

TEST_F(SuccessorMapTest, HeadAfterInitialConstruction) {
  EXPECT_EQ(SegmentIndex::Null, sut.head());
}

TEST_F(SuccessorMapTest, PrepareAfterInitialConstruction) {
  EXPECT_EQ(SegmentIndex::Null, sut.prepare(capacity));
  EXPECT_EQ(SegmentIndex::Null, sut.head());
}

TEST_F(SuccessorMapTest, FirstInsertion) {
  const auto initial_construction_sentinel = sut.prepare(capacity);
  const auto result = sut.insert_after(initial_construction_sentinel);
  EXPECT_EQ(0u, std::to_underlying(result));
  EXPECT_EQ(SegmentIndex::Null, sut.successor(result));
  EXPECT_EQ(0u, std::to_underlying(sut.head()));
}

TEST_F(SuccessorMapTest, HeadAfterFirstInsertion) {
  const auto initial_construction_sentinel = sut.prepare(capacity);
  [[maybe_unused]] const auto result =
      sut.insert_after(initial_construction_sentinel);
  EXPECT_EQ(0u, std::to_underlying(sut.head()));
}

TEST_F(SuccessorMapTest, PrepareAfterInsertion) {
  {
    const auto original_initial_construction_sentinel = sut.prepare(capacity);
    [[maybe_unused]] const auto first_insertion =
        sut.insert_after(original_initial_construction_sentinel);
  }
  const auto initial_construction_sentinel = sut.prepare(capacity);
  const auto result = sut.insert_after(initial_construction_sentinel);
  EXPECT_EQ(0u, std::to_underlying(result));
  EXPECT_EQ(SegmentIndex::Null, sut.successor(result));
  EXPECT_EQ(0u, std::to_underlying(sut.head()));
}

TEST_F(SuccessorMapTest, InsertBefore) {
  const auto initial_construction_sentinel = sut.prepare(capacity);

  const auto begin = sut.insert_after(initial_construction_sentinel);
  const auto end = sut.insert_after(begin);
  const auto middle = sut.insert_after(begin);

  EXPECT_EQ(0u, std::to_underlying(begin));
  EXPECT_EQ(1u, std::to_underlying(end));
  EXPECT_EQ(2u, std::to_underlying(middle));

  EXPECT_EQ(middle, sut.successor(begin));
  EXPECT_EQ(end, sut.successor(middle));
  EXPECT_EQ(SegmentIndex::Null, sut.successor(end));
  EXPECT_EQ(0u, std::to_underlying(sut.head()));
}

TEST_F(SuccessorMapTest, InsertAfter) {
  const auto initial_construction_sentinel = sut.prepare(capacity);

  const auto begin = sut.insert_after(initial_construction_sentinel);
  const auto middle = sut.insert_after(begin);
  const auto end = sut.insert_after(middle);

  EXPECT_EQ(0u, std::to_underlying(begin));
  EXPECT_EQ(1u, std::to_underlying(middle));
  EXPECT_EQ(2u, std::to_underlying(end));

  EXPECT_EQ(middle, sut.successor(begin));
  EXPECT_EQ(end, sut.successor(middle));
  EXPECT_EQ(SegmentIndex::Null, sut.successor(end));
  EXPECT_EQ(0u, std::to_underlying(sut.head()));
}

// ----------------------------------------------------------------------------
// Death Tests
// ----------------------------------------------------------------------------

struct SuccessorMapDeathTest : SuccessorMapTest {
  auto insert(std::size_t index) -> void {
    [[maybe_unused]] const auto result =
        sut.insert_after(static_cast<SegmentIndex>(index));
  }
};

TEST_F(SuccessorMapDeathTest, SuccessorOnEmptyInitialConstructionSentinel) {
  const auto initial_construction_sentinel = sut.prepare(capacity);
  EXPECT_DEATH(sut.successor(initial_construction_sentinel),
               "index out of range");
}

TEST_F(SuccessorMapTest, InitialConstructionSentinelReuse) {
  const auto initial_construction_sentinel = sut.prepare(capacity);

  const auto root = sut.insert_after(initial_construction_sentinel);
  EXPECT_EQ(0u, std::to_underlying(root));

  EXPECT_DEATH(
      [&]() { return sut.insert_after(initial_construction_sentinel); }(),
      "initial insertion sentinel reused");
}

TEST_F(SuccessorMapDeathTest, DefaultInitializedInsert) {
  EXPECT_DEATH(insert(capacity), "insert on full map");
}

TEST_F(SuccessorMapDeathTest, DefaultInitializedSuccessor) {
  EXPECT_DEATH(sut.successor(SegmentIndex{0}), "index out of range");
}

TEST_F(SuccessorMapDeathTest, InsertAfterOutOfRange) {
  [[maybe_unused]] const auto initial_construction_sentinel =
      sut.prepare(capacity);
  EXPECT_DEATH(insert(capacity), "index out of range");
}

TEST_F(SuccessorMapDeathTest, SuccessorOutOfRange) {
  [[maybe_unused]] const auto initial_construction_sentinel =
      sut.prepare(capacity);
  EXPECT_DEATH(sut.successor(SegmentIndex{capacity}), "index out of range");
}

TEST_F(SuccessorMapDeathTest, InsertOnFull) {
  [[maybe_unused]] const auto initial_construction_sentinel =
      sut.prepare(capacity);

  auto tail = sut.insert_after(initial_construction_sentinel);
  for (auto safe_iteration = 1u; safe_iteration < capacity; ++safe_iteration) {
    tail = sut.insert_after(tail);
  }

  EXPECT_DEATH(insert(0), "insert on full map");
}

}  // namespace
}  // namespace curves::spline::segment
