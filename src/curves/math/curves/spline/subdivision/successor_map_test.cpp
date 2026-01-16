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

TEST_F(SuccessorMapTest, ResetAfterInitialConstruction) {
  const auto root = sut.reset(capacity);
  EXPECT_EQ(0u, std::to_underlying(root));  // always starts at 0.
  EXPECT_EQ(SegmentIndex::Null, sut.successor(root));
}

TEST_F(SuccessorMapTest, FirstInsertion) {
  const auto root = sut.reset(capacity);
  const auto result = sut.insert_after(root);
  EXPECT_EQ(1u, std::to_underlying(result));
  EXPECT_EQ(result, sut.successor(root));
  EXPECT_EQ(SegmentIndex::Null, sut.successor(result));
}

TEST_F(SuccessorMapTest, ResetAfterFirstInsertion) {
  {
    const auto original_root = sut.reset(capacity);
    [[maybe_unused]] const auto first_insertion =
        sut.insert_after(original_root);
  }
  const auto result = sut.reset(capacity);
  EXPECT_EQ(SegmentIndex::Null, sut.successor(result));
}

TEST_F(SuccessorMapTest, InsertionBefore) {
  const auto root = sut.reset(capacity);
  const auto tail = sut.insert_after(root);
  const auto result = sut.insert_after(root);
  EXPECT_EQ(2u, std::to_underlying(result));
  EXPECT_EQ(result, sut.successor(root));
  EXPECT_EQ(tail, sut.successor(result));
  EXPECT_EQ(SegmentIndex::Null, sut.successor(tail));
}

TEST_F(SuccessorMapTest, InsertionAfter) {
  const auto root = sut.reset(capacity);
  const auto middle = sut.insert_after(root);
  const auto end = sut.insert_after(middle);

  EXPECT_EQ(1u, std::to_underlying(middle));
  EXPECT_EQ(2u, std::to_underlying(end));

  EXPECT_EQ(middle, sut.successor(root));
  EXPECT_EQ(end, sut.successor(middle));
  EXPECT_EQ(SegmentIndex::Null, sut.successor(end));
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

TEST_F(SuccessorMapDeathTest, insert_after_empty_map) {
  EXPECT_DEATH(insert(capacity), "insert on full map");
}

TEST_F(SuccessorMapDeathTest, insert_after_bad_index) {
  [[maybe_unused]] const auto root = sut.reset(capacity);
  EXPECT_DEATH(insert(capacity), "index out of range");
}

TEST_F(SuccessorMapDeathTest, successor_bad_index) {
  EXPECT_DEATH(sut.successor(SegmentIndex{capacity}), "index out of range");
}

TEST_F(SuccessorMapDeathTest, insert_after_full) {
  [[maybe_unused]] const auto root = sut.reset(capacity);

  for (auto safe_iteration = 1u; safe_iteration < capacity; ++safe_iteration) {
    insert(0);
  }

  EXPECT_DEATH(insert(0), "insert on full map");
}

}  // namespace
}  // namespace curves::spline::segment
