// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Smooth transition segment between two linear segments.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <cassert>

namespace curves::shaping::transition_functions {

/*!
  Composes a transition function to reflect it about the point (1, at_1(1)).
*/
template <typename TransitionFunction>
struct Reflected {
  using Scalar = TransitionFunction::Scalar;

  [[no_unique_address]] TransitionFunction transition_function;

  /*!
    \returns value of reflected function at x=1.

    Nominally, the result at x=1 is f(1) - f(1 - 0), but by definition,
    transition functions always return 0 at 0, so f(0) vanishes and the result
    is the same as the original function at x=1.
  */
  constexpr auto at_1() const noexcept { return transition_function.at_1(); }

  template <typename Value>
  constexpr auto operator()(const Value& t) const {
    return Value{transition_function.at_1()} -
           transition_function(Value{1} - t);
  }
};

}  // namespace curves::shaping::transition_functions
