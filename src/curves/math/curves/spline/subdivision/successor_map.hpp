// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Map of segment successors, ordered by index.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/curves/spline/subdivision/subdivision.hpp>
#include <cassert>
#include <vector>

namespace curves::spline::segment {

/*!
  Map of segment successors, ordered by index.

  This type represents the topology of a spline, mapping from a given segment
  index to its successor's index.
*/
class SuccessorMap {
 public:
  //! \returns Index of first valid segment, or Null if empty.
  [[nodiscard]] auto head() const noexcept -> SegmentIndex {
    return next_map_.empty() ? SegmentIndex::Null : SegmentIndex{0};
  }

  /*!
    Prepares map for a refinement pass; empty with given capacity.

    This function preallocates the array of successor indices. Because in our
    usage the maximum number of segments is small and known beforehand, we
    preallocate the entire array and assert if an attempt is made to grow it
    later.

    The return value is the initial insertion sentinel used to insert the first
    successor. It can only be used once.

    \param capacity maximum possible subdivisions
    \returns initial insertion sentinel
  */
  [[nodiscard]] auto prepare(std::size_t capacity) -> SegmentIndex {
    next_map_.clear();
    next_map_.reserve(capacity);
    return SegmentIndex::Null;
  }

  /*!
    Links a new segment immediately after the predecessor.
    \returns index of newly created segment
    \pre size < capacity
  */
  [[nodiscard]] auto insert_after(SegmentIndex predecessor) -> SegmentIndex {
    // Standard bounds check.
    assert(next_map_.size() < next_map_.capacity() &&
           "SuccessorMap: insert on full map");

    // The initial insertion sentinel can only be used once.
    assert((predecessor != SegmentIndex::Null || next_map_.empty()) &&
           "SuccessorMap: initial insertion sentinel reused");

    // Place segment physically at end of vector.
    const auto new_index = static_cast<SegmentIndex>(next_map_.size());
    if (predecessor != SegmentIndex::Null) {
      // Wire it in logically after predecessor. Standard list insertion.
      auto& prev_next = next_map_[to_map_index(predecessor)];
      next_map_.push_back(prev_next);  // cur.next = prev.next
      prev_next = new_index;           // prev.next = cur
    } else {
      // This is the first segment added. It has no successor or predecessor.
      next_map_.push_back(SegmentIndex::Null);  // cur.next = null
    }

    return new_index;
  }

  //! \returns index of segment's successor
  auto successor(SegmentIndex index) const noexcept -> SegmentIndex {
    return next_map_[to_map_index(index)];
  }

 private:
  std::vector<SegmentIndex> next_map_{};

  // Converts from enum type to index type, asserting on range.
  auto to_map_index(SegmentIndex segment_index) const noexcept -> std::size_t {
    const auto index = static_cast<std::size_t>(segment_index);
    assert(index < next_map_.size() && "SuccessorMap: index out of range");
    return index;
  }
};

}  // namespace curves::spline::segment
