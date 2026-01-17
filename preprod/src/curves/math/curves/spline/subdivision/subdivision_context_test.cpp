// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2026 Frank Secilia
*/

#include "subdivision_context.hpp"
#include <curves/testing/test.hpp>
#include <gmock/gmock.h>

namespace curves::spline::segment {
namespace {

struct SubdivisionContextTest : Test {
  struct RefinementQueue {
    MOCK_METHOD(void, prepare, (std::size_t capacity));
    virtual ~RefinementQueue() = default;
  };

  struct SuccessorMap {
    MOCK_METHOD(SegmentIndex, prepare, (std::size_t capacity));
    virtual ~SuccessorMap() = default;
  };

  using Sut =
      SubdivisionContext<StrictMock<RefinementQueue>, StrictMock<SuccessorMap>>;
  Sut sut{};
};

TEST_F(SubdivisionContextTest, prepare) {
  const auto capacity = std::size_t{10};
  const auto expected_result = SegmentIndex{3};

  EXPECT_CALL(sut.refinement_queue, prepare(capacity));
  EXPECT_CALL(sut.successor_map, prepare(capacity))
      .WillOnce(Return(expected_result));

  const auto actual_result = sut.prepare(capacity);

  EXPECT_EQ(expected_result, actual_result);
}

}  // namespace
}  // namespace curves::spline::segment
