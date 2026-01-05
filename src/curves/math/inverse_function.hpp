// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Methods of inverting functions.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <cassert>
#include <numeric>

namespace curves {

/*!
  Numerically inverts function via partitioning (binary search).

  This function solves y = f(x) for x given y. It uses a binary search to break
  the domain into smaller and smaller candidate regions that still contain the
  value until a threshold is reached.

  The given function must be monotonically increasing or decreasing over range
  from 0 to the first power of two above the target.

  This algorithm is not particularly efficient, but it's simple and sufficient
  for finding a handful of values at interactive rates. It is optimized for
  results closer to 0 than to the middle or end of the range of Value.

  @pre The function `f` must be monotonic over the domain [0, inf).
  @pre The target `y` must lie within the range mapped by f([0, inf)).
  @note This algorithm strictly searches for positive solutions (x >= 0).
*/
struct InverseViaPartition {
  template <typename Function, typename Value>
  constexpr auto operator()(Function&& f, const Value& y) const -> Value {
    auto x_lower = Value{0};
    auto x_upper = Value{1};
    const auto is_increasing = f(x_upper) > f(x_lower);

    // Clamp half range to 0.
    const auto y_start = f(Value{0});
    assert((is_increasing && y >= y_start) || (!is_increasing && y <= y_start));
    if (is_increasing && y < y_start) return Value{0};
    if (!is_increasing && y > y_start) return Value{0};

    // Bracket search location. Start with a small window, then shift it and
    // grow geometrically until we find a region containing the target.
    constexpr auto kMaxBrackets = 64;
    for (auto bracket = 0; bracket < kMaxBrackets; ++bracket) {
      const auto y_upper = f(x_upper);
      if (is_increasing ? (y_upper >= y) : (y_upper <= y)) break;
      x_lower = x_upper;
      x_upper *= 2.0L;
    }

    // Run standard binary search within the bracket.
    constexpr auto kMaxSearchIterations = 64;
    for (auto iteration = 0; iteration < kMaxSearchIterations; ++iteration) {
      const auto x_mid = std::midpoint(x_lower, x_upper);
      const auto y_mid = f(x_mid);

      if (is_increasing) {
        if (y < y_mid) {
          x_upper = x_mid;
        } else {
          x_lower = x_mid;
        }
      } else {
        if (y > y_mid) {
          x_upper = x_mid;
        } else {
          x_lower = x_mid;
        }
      }
    }

    return std::midpoint(x_lower, x_upper);
  }
};
inline constexpr auto inverse_via_partition = InverseViaPartition{};

}  // namespace curves
