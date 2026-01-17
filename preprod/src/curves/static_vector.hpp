// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Vector with static capacity.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <algorithm>
#include <array>
#include <cassert>
#include <initializer_list>
#include <ostream>

namespace curves {

//! Minimal vector-like container operating over static capacity.
template <typename Value, size_t Capacity>
class StaticVector {
 public:
  using Data = std::array<Value, Capacity>;

  using value_type = Data::value_type;
  using size_type = Data::size_type;
  using difference_type = Data::difference_type;
  using pointer = Data::pointer;
  using const_pointer = Data::const_pointer;
  using iterator = Data::iterator;
  using const_iterator = Data::const_iterator;

  constexpr StaticVector() = default;

  constexpr StaticVector(std::initializer_list<Value> init) noexcept {
    assert(init.size() <= Capacity &&
           "StaticVector: initializer_list exceeds capacity");
    std::copy(init.begin(), init.end(), data_.begin());
    size_ = init.size();
  }

  constexpr auto push_back(const Value& value) noexcept -> void {
    assert(size_ < Capacity && "StaticVector: overflow");
    data_[size_++] = value;
  }

  constexpr auto begin(this auto&& self) noexcept -> auto {
    return self.data_.begin();
  }

  constexpr auto end(this auto&& self) noexcept -> auto {
    return self.data_.begin() + self.size_;
  }

  constexpr auto size() const noexcept -> size_type { return size_; }
  constexpr auto empty() const noexcept -> bool { return size_ == 0; }
  constexpr auto max_size() const noexcept -> size_type { return Capacity; }

  constexpr auto operator[](this auto&& self, std::size_t index) noexcept
      -> decltype(auto) {
    assert(index < self.size_ && "StaticVector: index out of range");
    return self.data_[index];
  }

  friend auto operator<<(std::ostream& out, const StaticVector& src)
      -> std::ostream& {
    out << "{";

    auto next = std::begin(src);
    const auto end = std::end(src);
    if (next != end) {
      out << *next++;
      while (next != end) {
        out << ", " << *next++;
      }
    }

    out << "}";

    return out;
  }

 private:
  Data data_;
  size_type size_ = 0;
};

}  // namespace curves
