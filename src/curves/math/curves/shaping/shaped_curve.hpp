// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Composes a curve over ease-in and ease-out domain warps.

                                     |                  ________
                              /      |              .-''
                            /        |            /
           Ease-In        /          |          /      Ease-Out
                        /            |        /
          _________..-'              |      /
             flat |----| linear      |      linear |----| flat
                transition           |           transition

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <algorithm>
#include <vector>

namespace curves::shaping {

/*!
  Composes curve over ease-in and ease-out.
*/
template <typename Scalar, typename Curve, typename EaseIn, typename EaseOut>
class ShapedCurve {
 public:
  ShapedCurve(Curve curve, EaseIn ease_in, EaseOut ease_out) noexcept
      : curve_{std::move(curve)},
        ease_in_{std::move(ease_in)},
        ease_out_{std::move(ease_out)} {}

  template <typename Value>
  auto operator()(const Value& value) const noexcept -> Value {
    return curve_(ease_in_(ease_out_(value)));
  }

  using CriticalPoints = std::vector<Scalar>;
  auto critical_points(Scalar domain_max) const -> CriticalPoints {
    auto result = CriticalPoints{};

    /*
      For our current set of curves, the largest number of possible critical
      points after composition is 7. 2 each from input shaping, and synchronous
      has up to 3 if linear clamping is on.
    */
    result.reserve(7);

    for (const auto ease_out_critical_point : ease_out_.critical_points()) {
      try_add_critical_point(result, ease_out_critical_point, domain_max);
    }

    for (const auto ease_in_critical_point : ease_in_.critical_points()) {
      try_add_critical_point(result, ease_out_.inverse(ease_in_critical_point),
                             domain_max);
    }

    for (const auto curve_critical_point : curve_.critical_points()) {
      try_add_critical_point(
          result, ease_out_.inverse(ease_in_.inverse(curve_critical_point)),
          domain_max);
    }

    // Sort and deduplicate
    std::ranges::sort(result);
    const auto last = std::ranges::unique(result, [](auto a, auto b) {
                        return std::abs(a - b) < Scalar{1e-9};
                      }).begin();
    result.erase(last, result.end());

    return result;
  }

 private:
  [[no_unique_address]] Curve curve_{};
  [[no_unique_address]] EaseIn ease_in_{};
  [[no_unique_address]] EaseOut ease_out_{};

  auto is_in_domain(Scalar x, Scalar domain_max) const noexcept -> bool {
    return 0 <= x && x <= domain_max;
  }

  auto try_add_critical_point(CriticalPoints& critical_points,
                              Scalar critical_point, Scalar domain_max) const
      -> void {
    if (!is_in_domain(critical_point, domain_max)) return;
    critical_points.push_back(critical_point);
  }
};

}  // namespace curves::shaping
