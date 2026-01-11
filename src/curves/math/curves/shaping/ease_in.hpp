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

//
//                        /
//                      /
//                    /
//                  /
//    _________..-'
//       flat |----| linear
//          transition
//
template <typename Transition>
class EaseIn {
 public:
  using Scalar = Transition::Scalar;

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

  constexpr auto inverse(Scalar y) const noexcept -> Scalar {
    // Flat segment.
    if (y <= 0) return transition_.x0();

    // Linear segment.
    if (y >= transition_.height()) return y + lag_;

    // Transition segment.
    return transition_.inverse(y);
  }

  constexpr auto critical_points() const noexcept -> std::array<Scalar, 2> {
    const auto x0 = transition_.x0();
    return {x0, x0 + transition_.width()};
  }

 private:
  Scalar lag_;
  [[no_unique_address]] Transition transition_;
};

}  // namespace curves::shaping
