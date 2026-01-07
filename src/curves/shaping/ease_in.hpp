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
      : transition_{std::move(transition)},
        lag_{transition_.x0() + transition.width() - transition_.height()} {}

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

 private:
  [[no_unique_address]] Transition transition_;
  Parameter lag_;
};

}  // namespace curves::shaping
