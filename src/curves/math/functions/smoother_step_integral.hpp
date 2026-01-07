// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Defines the integral of smootherstep.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/jet.hpp>

namespace curves::math {

/*!
  This function is the integral of smootherstep. It gives C^3 continuity when
  concatenating between horizontal and linear segments.

    P(t) = t^6 - 3t^5 + 2.5t^4 = t^4(t^2 - 3t + 2.5)

    P(0) = 0      P(1) = 0.5     (area ratio)
    P'(0) = 0     P'(1) = 1      (slope continuity)
    P''(0) = 0    P''(1) = 0     (curvature continuity)
    P'''(0) = 0   P'''(1) = 0    (jerk continuity)

  Evaluating this curve at x=1 yields the jet {0.5, 1}, *NOT* {1, 1}!
*/
struct SmootherStepIntegral {
  static constexpr auto kC1 = -3.0;
  static constexpr auto kC2 = 2.5;

  // \pre t in [0, 1]
  template <typename Value>
  constexpr Value operator()(const Value& t) const {
    const auto t2 = t * t;
    const auto t4 = t2 * t2;
    return t4 * (t2 + (t * Value{kC1}) + Value{kC2});
  }
};

}  // namespace curves::math
