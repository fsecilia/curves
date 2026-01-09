// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2026 Frank Secilia
*/

#include "ease_in.hpp"
#include <curves/testing/test.hpp>
#include <curves/math/jet.hpp>
#include <curves/shaping/ease_testing.hpp>
#include <curves/shaping/transition.hpp>
#include <curves/shaping/transition_functions/smoother_step_integral.hpp>
#include <gmock/gmock.h>

namespace curves::shaping {
namespace {

// ============================================================================
// operator()
// ============================================================================

// ----------------------------------------------------------------------------
// Fixture
// ----------------------------------------------------------------------------

template <typename Transition>
struct EaseInCallTest : TestWithParam<CallTestVector> {
  using Sut = EaseIn<Parameter, Transition, Inverter>;
  Sut sut{{}, {}};

  auto test() const -> void {
    const auto x = Jet{GetParam().x, 1.0};
    const auto expected = GetParam().expected;

    const auto actual = sut(x);

    EXPECT_NEAR(expected.a, actual.a, 1e-10);
    EXPECT_NEAR(expected.v, actual.v, 1e-10);
  }
};

// ----------------------------------------------------------------------------
// Nominal Case
// ----------------------------------------------------------------------------

namespace nominal {

/*
  These values end up being discontinuous at the end of the transition, but the
  math still works if you sample segment by segment.
*/
constexpr auto x0 = Parameter{0.1};
constexpr auto width = Parameter{1.2};
constexpr auto height = Parameter{2.5};
constexpr auto slope = height / width;

struct EaseInCallTestNominal
    : EaseInCallTest<TestingTransition<x0, width, height>> {};

TEST_P(EaseInCallTestNominal, Parameterized) { test(); }

constexpr CallTestVector test_vectors[] = {
    // Well out of domain to left.
    {-1, {0, 0}},

    // 0, flat segment begin
    {-kEps, {0, 0}},
    {0, {0, 0}},
    {kEps, {0, 0}},

    // Flat segment end, transition segment begin.
    {x0 - kEps, {0, 0}},
    {x0, {0, slope}},
    {x0 + kEps, {slope * kEps, slope}},

    // Transition segment midpoint.
    {x0 + width / 2, {slope * width / 2, slope}},

    // Transition segment end, linear segment begin.
    {x0 + width - kEps, {slope * (width - kEps), slope}},
    {x0 + width, {height, 1}},
    {x0 + width + kEps, {height + kEps, 1}},

    // Linear segment interior.
    {x0 + width + 10, {height + 10, 1}},
};
INSTANTIATE_TEST_SUITE_P(TestVectors, EaseInCallTestNominal,
                         ValuesIn(test_vectors));

}  // namespace nominal

// ----------------------------------------------------------------------------
// Zero x0: flat segment vanishes
// ----------------------------------------------------------------------------

namespace zero_x0 {

constexpr auto x0 = Parameter{0};
constexpr auto width = Parameter{2};
constexpr auto height = Parameter{3};
constexpr auto slope = height / width;

struct EaseInCallTestZeroX0
    : EaseInCallTest<TestingTransition<x0, width, height>> {};

TEST_P(EaseInCallTestZeroX0, Parameterized) { test(); }

constexpr CallTestVector test_vectors[] = {
    // Before transition. This is out of the domain.
    {-kEps, {0, 0}},

    // At transition.
    {0, {0, slope}},

    // After transition.
    {kEps, {kEps * slope, slope}},
};
INSTANTIATE_TEST_SUITE_P(TestVectors, EaseInCallTestZeroX0,
                         ValuesIn(test_vectors));

}  // namespace zero_x0

// ----------------------------------------------------------------------------
// Zero width: transition segment vanishes
// ----------------------------------------------------------------------------

namespace zero_width {

constexpr auto x0 = Parameter{0.5};

struct Transition : DegenerateTransition {
  constexpr auto x0() const noexcept -> Parameter { return zero_width::x0; }
};

struct EaseInCallTestZeroWidth : EaseInCallTest<Transition> {};

TEST_P(EaseInCallTestZeroWidth, Parameterized) { test(); }

constexpr CallTestVector test_vectors[] = {
    // Flat segment.
    {x0 - kEps, {0, 0}},

    // Linear segment begins immediately at x0.
    {x0, {0, 1}},

    // Linear segment.
    {x0 + kEps, {kEps, 1}},
};
INSTANTIATE_TEST_SUITE_P(TestVectors, EaseInCallTestZeroWidth,
                         ValuesIn(test_vectors));

}  // namespace zero_width

// ----------------------------------------------------------------------------
// Null Transition: no transition at all
// ----------------------------------------------------------------------------

namespace null_transition {

struct Transition : DegenerateTransition {
  constexpr auto x0() const noexcept -> Parameter { return 0; }
};

struct EaseInCallTestNullTransition : EaseInCallTest<Transition> {};

TEST_P(EaseInCallTestNullTransition, Parameterized) { test(); }

constexpr CallTestVector test_vectors[] = {
    // Before what would be either the flat segment or the transition.
    {-kEps, {0, 0}},

    // Linear segment begins immediately at 0.
    {0, {0, 1}},

    // Linear segment.
    {kEps, {kEps, 1}},
};
INSTANTIATE_TEST_SUITE_P(TestVectors, EaseInCallTestNullTransition,
                         ValuesIn(test_vectors));

}  // namespace null_transition

}  // namespace

// ============================================================================
// inverse()
// ============================================================================

namespace inverse {
namespace {

struct EaseInInverseTest : Test {
  static constexpr auto x0 = Parameter{1};
  static constexpr auto width = Parameter{1};
  static constexpr auto height = Parameter{1};

  StrictMock<MockTransition> mock_transition;

  using Transition = inverse::Transition<x0, width, height>;

  using Sut = EaseIn<Parameter, Transition, Inverter>;
  Sut sut{Transition{{}, &mock_transition}, inverter};
};

TEST_F(EaseInInverseTest, FlatSegment) {
  const auto y = 0;
  const auto expected = x0;

  const auto actual = sut.inverse(y);

  EXPECT_DOUBLE_EQ(expected, actual);
}

TEST_F(EaseInInverseTest, LinearSegment) {
  const auto y = height + 1;
  const auto lag = x0 + width - height;
  const auto expected = y + lag;

  const auto actual = sut.inverse(y);

  EXPECT_DOUBLE_EQ(expected, actual);
}

TEST_F(EaseInInverseTest, TransitionSegment) {
  const auto y = height / 2;
  const auto expected = 17;
  EXPECT_CALL(mock_transition, inverse(y, inverter)).WillOnce(Return(expected));

  const auto actual = sut.inverse(y);

  EXPECT_DOUBLE_EQ(expected, actual);
}

}  // namespace
}  // namespace inverse

namespace {

// ============================================================================
// critical_points()
// ============================================================================

struct EaseInCriticalPointsTest : Test {
  static constexpr auto x0 = Parameter{2};
  static constexpr auto width = Parameter{5};
  static constexpr auto height = Parameter{11};

  using Sut = EaseIn<Parameter, TestingTransition<x0, width, height>, Inverter>;
  Sut sut{{}, {}};
};

TEST_F(EaseInCriticalPointsTest, CriticalPoints) {
  const auto expected = std::array{x0, x0 + width};

  const auto actual = sut.critical_points();

  ASSERT_EQ(expected, actual);
}

// ============================================================================
// Continuity
// ============================================================================

/*
  In one sense, this test is an integration test because it pulls in the
  production transition function instead of using a test double to isolate the
  test. However, what this test needs is a C3 curve, and rather than trying to
  make one just for the test, we use the one we already have laying around.
  It just so happens to be the same one we use in prod, but that's more
  coincidental than deliberate.
*/

struct EaseInContinuityTest : Test {
  using TransitionFunction =
      transition_functions::SmootherStepIntegral<Parameter>;
  using Transition = shaping::Transition<Parameter, TransitionFunction>;
  using Sut = EaseIn<Parameter, Transition, Inverter>;

  static constexpr auto x0 = Parameter{0.45};
  static constexpr auto width = Parameter{2.1};

  static constexpr auto transition = Transition{x0, width};
  static constexpr auto sut = Sut{transition, {}};

  static constexpr auto height = transition.height();
};

TEST_F(EaseInContinuityTest, AtX0) {
  const auto y = sut(Jet{x0, 1.0});

  EXPECT_DOUBLE_EQ(0.0, y.a);
  EXPECT_DOUBLE_EQ(0.0, y.v);
}

TEST_F(EaseInContinuityTest, AtX0PlusWidth) {
  const auto y = sut(Jet{x0 + width, 1.0});

  EXPECT_DOUBLE_EQ(height, y.a);
  EXPECT_DOUBLE_EQ(1.0, y.v);
}

}  // namespace
}  // namespace curves::shaping
