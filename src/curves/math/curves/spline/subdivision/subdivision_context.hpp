// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Mutable context used in subdivision algorithm.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/curves/spline/subdivision/refinement_queue.hpp>
#include <curves/math/curves/spline/subdivision/subdivision.hpp>
#include <curves/math/curves/spline/subdivision/successor_map.hpp>
#include <vector>

namespace curves::spline::segment {

//! Mutable context used in subdivision algorithm.
template <typename RefinementQueue, typename SuccessorMap>
struct SubdivisionContext {
  std::vector<Segment> segments;
  RefinementQueue refinement_queue;
  SuccessorMap successor_map;

  auto prepare(std::size_t capacity) -> SegmentIndex {
    segments.clear();
    segments.reserve(capacity);
    refinement_queue.prepare(capacity);
    return successor_map.prepare(capacity);
  }
};

}  // namespace curves::spline::segment
