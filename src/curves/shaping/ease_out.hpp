// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Piecewise ease-out function.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <utility>

namespace curves::shaping {

template <typename Parameter, typename Transition>
class EaseOut {
 public:
  explicit constexpr EaseOut(Transition transition) noexcept
      : transition_{std::move(transition)},
        ceiling_{transition_.x0() + transition.height()} {}

  template <typename Value>
  constexpr auto operator()(const Value& x) const noexcept -> Value {
    // Linear segment through origin.
    const auto x0 = Value{transition_.x0()};
    if (x < x0) return x;

    // Flat segment.
    const auto width = Value{transition_.width()};
    if (x >= x0 + width) return Value{ceiling_};

    // Transition segment.
    return transition_(x) + x0;
  }

  constexpr auto inverse(Parameter y, auto&& inverter) const noexcept
      -> Parameter {
    // Linear segment.
    if (y <= transition_.x0()) return y;

    // Flat segment.
    if (y >= ceiling_) return transition_.x0() + transition_.width();

    // Transition segment.
    return transition_.inverse(y - transition_.x0(),
                               std::forward<decltype(inverter)>(inverter));
  }

  constexpr auto critical_points() const noexcept -> std::array<Parameter, 2> {
    const auto x0 = transition_.x0();
    return {x0, x0 + transition_.width()};
  }

 private:
  [[no_unique_address]] Transition transition_;
  Parameter ceiling_;
};

}  // namespace curves::shaping
