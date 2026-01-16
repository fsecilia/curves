// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Priority queue of segments, ordered by max error.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/curves/spline/subdivision/subdivision.hpp>
#include <algorithm>
#include <cassert>
#include <vector>

namespace curves::spline::segment {

struct SegmentError {
  real_t error;
  SegmentIndex index;

  //! Orders by increasing segment error.
  auto operator<(const SegmentError& other) const noexcept -> bool {
    return error < other.error;
  }
};

/*!
  Priority queue of refinement work items, ordered by decreasing segment error.

  This queue maintains a list of candidate segments for splitting, in best-fit
  order. A segment is a candidate if its error exceeds tolerance and it is
  wider than the minimum width.

  Best fit order ensures the sharpest features are refined first without
  starving the rest of the spline for segments.
*/
template <typename WorkItem>
class RefinementQueue {
 public:
  auto empty() const noexcept -> bool { return work_items_.empty(); }

  /*!
    Prepares the queue for a new refinement pass.

    This function clears the clears any current items and pre-allocates to
    avoid reallocations.
  */
  auto prepare(std::size_t capacity) {
    work_items_.clear();
    work_items_.reserve(capacity);
  }

  auto push(WorkItem work_item) noexcept -> void {
    assert(work_items_.size() < work_items_.capacity() &&
           "RefinementQueue: push on full queue");

    work_items_.emplace_back(std::move(work_item));
    std::ranges::push_heap(work_items_);
  }

  auto pop() noexcept -> WorkItem {
    assert(!empty() && "RefinementQueue: pop on empty queue");

    std::ranges::pop_heap(work_items_);
    auto result = std::move(work_items_.back());
    work_items_.pop_back();

    return result;
  }

 private:
  std::vector<WorkItem> work_items_;
};

}  // namespace curves::spline::segment
