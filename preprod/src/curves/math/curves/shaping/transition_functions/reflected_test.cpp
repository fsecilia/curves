// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2026 Frank Secilia
*/

#include "reflected.hpp"
#include <curves/testing/test.hpp>
#include <curves/math/jet.hpp>

namespace curves::shaping::transition_functions {
namespace {

using Jet = math::Jet<real_t>;

static constexpr auto m = 2.1;
static constexpr auto x0 = 13.5;

const real_t test_vectors[] = {
    0.0 / 4, 1.0 / 4, 2.0 / 4, 3.0 / 4, 4.0 / 4,
};

// ============================================================================
// Test Doubles
// ============================================================================

struct LinearTransitionFunction {
  constexpr auto at_1() const noexcept -> real_t { return m * (1 - x0); }

  template <typename Value>
  constexpr auto operator()(const Value& t) const noexcept -> Value {
    return m * (t - x0);
  }
};

// ============================================================================
// ReflectedTransitionFunction
// ============================================================================

struct ReflectedTransitionTest : Test {
  using TransitionFunction = LinearTransitionFunction;
  static constexpr TransitionFunction transition_function{};

  using Sut = Reflected<TransitionFunction>;
  static constexpr Sut sut{transition_function};
};

TEST_F(ReflectedTransitionTest, at_1) {
  EXPECT_DOUBLE_EQ(transition_function.at_1(), sut.at_1());
}

// ----------------------------------------------------------------------------
// Parameterized Test
// ----------------------------------------------------------------------------

struct ReflectedTransitionParameterizedTest : ReflectedTransitionTest,
                                              WithParamInterface<real_t> {};

TEST_P(ReflectedTransitionParameterizedTest, evaluate) {
  const auto x = Jet{GetParam(), 1.0};
  const auto expected = transition_function.at_1() - m * ((1 - x) - x0);

  const auto actual = sut(x);

  EXPECT_EQ(expected, actual);
}

INSTANTIATE_TEST_SUITE_P(ReflectedTransitionTest,
                         ReflectedTransitionParameterizedTest,
                         ValuesIn(test_vectors));

}  // namespace
}  // namespace curves::shaping::transition_functions
