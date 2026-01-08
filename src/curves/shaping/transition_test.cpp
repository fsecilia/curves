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

using Parameter = double;
using Jet = math::Jet<Parameter>;

static constexpr auto m = 2.1;
static constexpr auto x0 = 10.0;
static constexpr auto width = 5.0;

const Parameter test_vectors[] = {
    0.0 / 4, 1.0 / 4, 2.0 / 4, 3.0 / 4, 4.0 / 4,
};

// ============================================================================
// Test Doubles
// ============================================================================

/*
  Linear Transition Function

  This test double returns the input directly, y = t.
  It creates a 1:1 linear system, isolating transition logic.
  With a linear transition function, the transition becomes:

    output = ((x - x0) / width) * scale

  Since scaling is uniform, scale = width:

    output = x - x0
*/
struct LinearTransitionFunction {
  constexpr auto at_1() const noexcept -> Parameter { return m; }

  template <typename Value>
  constexpr auto operator()(const Value& t) const noexcept -> Value {
    return m * t;
  }
};

// ============================================================================
// Transition
// ============================================================================

struct TransitionTest : Test {
  using Sut = Transition<Parameter, LinearTransitionFunction>;
  static constexpr Sut sut{x0, width};
};

TEST_F(TransitionTest, x0) { EXPECT_DOUBLE_EQ(x0, sut.x0()); }
TEST_F(TransitionTest, Width) { EXPECT_DOUBLE_EQ(width, sut.width()); }
TEST_F(TransitionTest, Height) { EXPECT_DOUBLE_EQ(m * width, sut.height()); }

// ----------------------------------------------------------------------------
// Parameterized Test
// ----------------------------------------------------------------------------

struct TransitionParameterizedTest : TransitionTest,
                                     WithParamInterface<Parameter> {};

TEST_P(TransitionParameterizedTest, evaluate) {
  const auto x = Jet{GetParam(), 1.0};
  const auto expected = m * (x - x0);

  const auto actual = sut(x);

  EXPECT_NEAR(expected.a, actual.a, 1e-10);
  EXPECT_NEAR(expected.v, actual.v, 1e-10);
}

INSTANTIATE_TEST_SUITE_P(TransitionTest, TransitionParameterizedTest,
                         ValuesIn(test_vectors));

// ----------------------------------------------------------------------------
// Death Tests
// ----------------------------------------------------------------------------

#if !defined NDEBUG

struct TransitionDeathTest : Test {
  using Sut = Transition<double, LinearTransitionFunction>;
  Sut sut{0, 0};
};

TEST_F(TransitionDeathTest, Evaluate) {
  EXPECT_DEBUG_DEATH(sut(0.0), "domain error");
}

#endif

}  // namespace
}  // namespace curves::shaping
