// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2026 Frank Secilia
*/

#include "transition.hpp"
#include <curves/testing/test.hpp>
#include <curves/math/jet.hpp>

namespace curves::shaping {
namespace {

/*
  Linear Transition Function

  This test double returns the input directly, y = t.
  It creates a 1:1 linear system, isolating transition logic.
  With a linear transition function, the transition becomes:

    output = ((x - x0) / width) * scale

  Since scaling is uniform, scale = width:

    output = x - x0
*/
struct LinearTransition {
  template <typename Value>
  constexpr auto operator()(const Value& t) const noexcept -> Value {
    return t;
  }
};

struct TransitionTest : Test {
  using Sut = Transition<double, LinearTransition>;

  // x0 = 10, width = 5, so f(x) = x - 10
  static constexpr Sut sut{10.0, 5.0};
};

// Calc scalars using linear transition.
TEST_F(TransitionTest, LinearTransitionMapsInputToOutputLinearly) {
  EXPECT_DOUBLE_EQ(0.0, sut(10.0));  // Start
  EXPECT_DOUBLE_EQ(2.5, sut(12.5));  // Mid
  EXPECT_DOUBLE_EQ(5.0, sut(15.0));  // End
}

// Calc jets using linear transition.
TEST_F(TransitionTest, JetsPropagateDerivative) {
  using Jet = math::Jet<double>;

  // Input: Value 12.5, Slope 1.0
  const auto input = Jet{12.5, 1.0};
  const auto output = sut(input);

  // f(x) = x - x0, f'(x) = 1
  EXPECT_DOUBLE_EQ(output.a, 2.5);
  EXPECT_DOUBLE_EQ(output.v, 1.0);
}

}  // namespace
}  // namespace curves::shaping
