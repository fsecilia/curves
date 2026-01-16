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
struct SubdivisionContext {
  std::vector<Segment> segments;
  RefinementQueue<SegmentError> refinement_queue;
  SuccessorMap successor_map;
};

}  // namespace curves::spline::segment
