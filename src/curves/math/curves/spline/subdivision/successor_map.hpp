// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Segment adjacency list, the topology of a spline.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/curves/spline/subdivision/subdivision.hpp>
#include <cassert>
#include <utility>
#include <vector>

namespace curves {

/*!
  Montonic index-based list over a vector.

  This type maintains a vector of segments that are linked internally by index.
  It supports reset(), but grows monotonically.
*/
template <typename Segment>
class SegmentList {
 public:
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using value_type = Segment;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using reference = value_type&;
  using const_reference = const value_type&;

  template <typename List, typename Reference>
  struct ListIterator {
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = std::remove_cvref_t<Reference>;
    using pointer = value_type*;
    using reference = Reference;

    List* list;
    NodeId current;

    auto operator*() const noexcept -> Reference { return (*list)[current]; }

    auto operator++() noexcept -> ListIterator& {
      current = list->next(current);
      return *this;
    }

    auto operator++(int) -> ListIterator {
      auto result = *this;
      ++*this;
      return result;
    }

    auto operator==(const ListIterator& other) const noexcept -> bool {
      return current == other.current;
    }
  };
  using iterator = ListIterator<SegmentList, reference>;
  using const_iterator = ListIterator<const SegmentList, const_reference>;

  explicit SegmentList(std::size_t capacity) { nodes_.reserve(capacity); }

  auto head() const noexcept -> NodeId { return head_; }
  auto tail() const noexcept -> NodeId { return tail_; }
  auto size() const noexcept -> size_t { return nodes_.size(); }
  auto capacity() const noexcept -> size_t { return nodes_.capacity(); }
  auto empty() const noexcept -> bool { return nodes_.empty(); }

  auto begin() const -> const_iterator { return {this, head_}; }
  auto end() const -> const_iterator { return {this, NodeId::Null}; }
  auto begin() -> iterator { return {this, head_}; }
  auto end() -> iterator { return {this, NodeId::Null}; }

  template <typename Self>
  auto operator[](this Self&& self, NodeId id) noexcept -> decltype(auto) {
    assert(static_cast<std::size_t>(id) < self.nodes_.size());
    return (std::forward<Self>(self).nodes_[static_cast<std::size_t>(id)].data);
  }

  auto next(NodeId id) const noexcept -> NodeId {
    return nodes_[static_cast<size_t>(id)].next;
  }

  auto reset(size_t capacity) -> void {
    nodes_.clear();
    if (nodes_.capacity() < capacity) nodes_.reserve(capacity);

    head_ = NodeId::Null;
    tail_ = NodeId::Null;
  }

  //! Appends after tail. Returns Null if capacity is full.
  auto push_back(Segment&& value) noexcept -> NodeId {
    if (nodes_.size() == nodes_.capacity()) return NodeId::Null;

    // Create new node.
    const auto new_id = allocate(std::move(value));

    // Wire it in.
    if (head_ == NodeId::Null) {
      head_ = new_id;
      tail_ = new_id;
    } else {
      nodes_[static_cast<size_t>(tail_)].next = new_id;
      tail_ = new_id;
    }

    return new_id;
  }

  //! Inserts after parent_id. Returns Null if capacity is full.
  auto insert_after(NodeId parent_id, Segment&& value) -> NodeId {
    if (nodes_.size() == nodes_.capacity()) return NodeId::Null;

    // Create new node.
    auto new_id = allocate(std::move(value));
    auto& new_node = nodes_.back();

    // Wire it in.
    auto& parent_node = nodes_[static_cast<size_t>(parent_id)];
    new_node.next = parent_node.next;
    parent_node.next = new_id;
    if (parent_id == tail_) tail_ = new_id;

    return new_id;
  }

 private:
  struct Node {
    Segment data;
    NodeId next{NodeId::Null};
  };

  auto allocate(Segment&& value) -> NodeId {
    nodes_.push_back({std::move(value), NodeId::Null});
    return static_cast<NodeId>(nodes_.size() - 1);
  }

  std::vector<Node> nodes_;
  NodeId head_{NodeId::Null};
  NodeId tail_{NodeId::Null};
};

}  // namespace curves
