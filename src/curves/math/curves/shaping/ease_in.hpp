// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Piecewise ease-in function.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <utility>

namespace curves::shaping {

template <typename Parameter, typename Transition>
class EaseIn {
 public:
  explicit constexpr EaseIn(Transition transition) noexcept
      : lag_{transition.x0() + transition.width() - transition.height()},
        transition_{std::move(transition)} {}

  template <typename Value>
  constexpr auto operator()(const Value& x) const noexcept -> Value {
    // Flat segment.
    const auto x0 = Value{transition_.x0()};
    if (x < x0) return Value{0};

    // Linear segment.
    const auto width = Value{transition_.width()};
    if (x >= x0 + width) return x - Value{lag_};

    // Transition segment.
    return transition_(x);
  }

  constexpr auto inverse(Parameter y) const noexcept -> Parameter {
    // Flat segment.
    if (y <= 0) return transition_.x0();

    // Linear segment.
    if (y >= transition_.height()) return y + lag_;

    // Transition segment.
    return transition_.inverse(y);
  }

  constexpr auto critical_points() const noexcept -> std::array<Parameter, 2> {
    const auto x0 = transition_.x0();
    return {x0, x0 + transition_.width()};
  }

 private:
  Parameter lag_;
  [[no_unique_address]] Transition transition_;
};

}  // namespace curves::shaping
