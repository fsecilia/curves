// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "static_vector.hpp"
#include <curves/testing/test.hpp>
#include <numeric>
#include <ranges>
#include <sstream>

namespace curves {
namespace {

//! Tests that sut functions as a range.
template <typename Range>
auto sum_range(const Range& r) {
  using T = std::ranges::range_value_t<Range>;
  return std::accumulate(r.begin(), r.end(), T{0});
}

TEST(StaticVectorTest, InitializerListConstruction) {
  const auto vec = StaticVector<float, 5>{1.0f, 2.0f, 3.0f};

  EXPECT_EQ(vec.size(), 3);
  EXPECT_FALSE(vec.empty());
  EXPECT_FLOAT_EQ(vec[0], 1.0f);
  EXPECT_FLOAT_EQ(vec[2], 3.0f);
}

TEST(StaticVectorTest, PushBackLogic) {
  StaticVector<int, 3> vec;
  EXPECT_TRUE(vec.empty());

  vec.push_back(10);
  vec.push_back(20);

  EXPECT_EQ(vec.size(), 2);
  EXPECT_EQ(vec[1], 20);
}

TEST(StaticVectorTest, WorksWithRanges) {
  const auto vec = StaticVector<double, 4>{0.5, 0.5, 1.0};

  const auto sum = sum_range(vec);

  EXPECT_DOUBLE_EQ(sum, 2.0);
}

TEST(StaticVectorTest, OstreamInserter) {
  auto out = std::ostringstream{};

  out << StaticVector<int, 4>{1, 2, 3};

  EXPECT_EQ("{1, 2, 3}", out.str());
}

TEST(StaticVectorTest, OstreamInserterEmpty) {
  auto out = std::ostringstream{};

  out << StaticVector<int, 4>{};

  EXPECT_EQ("{}", out.str());
}

#ifndef NDEBUG
TEST(StaticVectorTest, DeathOnOverflow) {
  StaticVector<int, 1> vec;
  vec.push_back(1);
  EXPECT_DEATH(vec.push_back(2), "overflow");
}
#endif

}  // namespace
}  // namespace curves
