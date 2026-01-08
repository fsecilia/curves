// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Smooth transition segment between two linear segments.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <cassert>
#include <utility>

namespace curves::shaping {

/*!
  Smooth transition between two linear segments using a normalized transition
  function.

  Transition functions are normalized to the domain [0, 1), but not the range
  [0, 1). They must go through (0, 0) with slope 0 and have slope 1 at x=1,
  but may go through any y at x=1.
*/
template <typename Parameter, typename TransitionFunction>
class Transition {
 public:
  constexpr Transition(Parameter x0, Parameter width,
                       TransitionFunction transition_function = {}) noexcept
      : x0_{x0},
        inv_width_{Parameter{1} / width},
        scale_{width},
        transition_function_{std::move(transition_function)} {}

  /*!
    \pre width > 0, x in [x0, x0 + width).
    \returns value of transition function scaled to this segment.
  */
  template <typename Value>
  constexpr auto operator()(const Value& x) const noexcept -> Value {
    assert(scale_ != Parameter{0} && "Transition domain error");

    // Reduce to [0, 1).
    const auto input = (x - Value{x0_}) * Value{inv_width_};

    // Apply normalized transition.
    const auto output = transition_function_(input);

    // Restore to original range.
    return output * Value{scale_};
  }

  constexpr auto x0() const noexcept -> Parameter { return x0_; }

  constexpr auto width() const noexcept -> Parameter {
    // Scale is uniform in width and height.
    return scale_;
  }

  constexpr auto height() const noexcept -> Parameter {
    return scale_ * transition_function_.at_1();
  }

 private:
  //! Beginning of transition.
  Parameter x0_;

  //! Reciprocal of width of transition.
  Parameter inv_width_;

  //! Uniform output scale to match input width 1:1.
  Parameter scale_;

  //! Actual easing implementation.
  [[no_unique_address]] TransitionFunction transition_function_;
};

}  // namespace curves::shaping
