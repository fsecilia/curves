// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Defines the integral of smootherstep as a transition function.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>

namespace curves::shaping::transition_functions {

/*!
  This transition function is the integral of smootherstep. It gives C^3
  continuity when concatenating between horizontal and linear segments.

    P(t) = t^6 - 3t^5 + 2.5t^4 = t^4(t^2 - 3t + 2.5)

    P(0) = 0      P(1) = 0.5     (area ratio)
    P'(0) = 0     P'(1) = 1      (slope continuity)
    P''(0) = 0    P''(1) = 0     (curvature continuity)
    P'''(0) = 0   P'''(1) = 0    (jerk continuity)

  Evaluating this curve at x=1 yields the jet {0.5, 1}, *NOT* {1, 1}!
*/
struct SmootherStepIntegral {
  static constexpr real_t kC0 = 1.0;
  static constexpr real_t kC1 = -3.0;
  static constexpr real_t kC2 = 2.5;

  // Optimized result without rounding.
  constexpr auto at_1() const noexcept -> real_t { return kC0 + kC1 + kC2; }

  // \pre t in [0, 1]
  template <typename Value>
  constexpr Value operator()(const Value& t) const {
    const auto t2 = t * t;
    const auto t4 = t2 * t2;
    return t4 * (t2 + (t * Value{kC1}) + Value{kC2});
  }
};

}  // namespace curves::shaping::transition_functions
