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

// ----------------------------------------------------------------------------
// Test Doubles
// ----------------------------------------------------------------------------

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
  static constexpr auto slope = 2.1;

  constexpr auto at_1() const noexcept -> double { return slope; }

  template <typename Value>
  constexpr auto operator()(const Value& t) const noexcept -> Value {
    return slope * t;
  }
};

// ----------------------------------------------------------------------------
// Test Fixture
// ----------------------------------------------------------------------------

struct TransitionTest : Test {
  using Sut = Transition<double, LinearTransition>;

  static constexpr auto m = LinearTransition::slope;
  static constexpr auto x0 = 10.0;
  static constexpr auto width = 5.0;

  static constexpr Sut sut{x0, width};
};

TEST_F(TransitionTest, x0) { EXPECT_DOUBLE_EQ(x0, sut.x0()); }
TEST_F(TransitionTest, Width) { EXPECT_DOUBLE_EQ(width, sut.width()); }
TEST_F(TransitionTest, Height) { EXPECT_DOUBLE_EQ(m * width, sut.height()); }

// Calc scalars using linear transition.
TEST_F(TransitionTest, LinearTransitionMapsInputToOutputLinearly) {
  EXPECT_DOUBLE_EQ(m * 0.0, sut(10.0));  // Start
  EXPECT_DOUBLE_EQ(m * 2.5, sut(12.5));  // Mid
  EXPECT_DOUBLE_EQ(m * 5.0, sut(15.0));  // End
}

// Calc jets using linear transition.
TEST_F(TransitionTest, JetsPropagateDerivative) {
  using Jet = math::Jet<double>;

  const auto input = Jet{12.5, 1.0};
  const auto output = sut(input);

  EXPECT_DOUBLE_EQ(output.a, m * 2.5);
  EXPECT_DOUBLE_EQ(output.v, m * 1.0);
}

// ----------------------------------------------------------------------------
// Death Tests
// ----------------------------------------------------------------------------

#if !defined NDEBUG

struct TransitionDeathTest : Test {
  using Sut = Transition<double, LinearTransition>;
  Sut sut{0, 0};
};

TEST_F(TransitionDeathTest, Evaluate) {
  EXPECT_DEBUG_DEATH(sut(0.0), "domain error");
}

#endif

}  // namespace
}  // namespace curves::shaping
