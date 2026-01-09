// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2026 Frank Secilia
*/

#include "ease_out.hpp"
#include <curves/testing/test.hpp>
#include <curves/math/jet.hpp>
#include <curves/shaping/ease_testing.hpp>
#include <curves/shaping/transition.hpp>
#include <curves/shaping/transition_functions/reflected.hpp>
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
struct EaseOutCallTest : TestWithParam<CallTestVector> {
  using Sut = EaseOut<Parameter, Transition, Inverter>;
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

  The test transition is linear: y = (x - x0) * slope, where slope =
  height/width. EaseOut adds x0 to this in the transition segment: y = x0 + (x -
  x0) * slope.
*/
constexpr auto x0 = Parameter{0.1};
constexpr auto width = Parameter{1.2};
constexpr auto height = Parameter{2.5};
constexpr auto slope = height / width;
constexpr auto ceiling = x0 + height;

struct EaseOutCallTestNominal
    : EaseOutCallTest<TestingTransition<x0, width, height>> {};

TEST_P(EaseOutCallTestNominal, Parameterized) { test(); }

constexpr CallTestVector test_vectors[] = {
    // Well out of domain to left.
    {-1, {-1, 1}},

    // Linear segment interior.
    {0, {0, 1}},

    // Linear segment end, transition segment begin.
    {x0 - kEps, {x0 - kEps, 1}},
    {x0, {x0, slope}},
    {x0 + kEps, {x0 + slope * kEps, slope}},

    // Transition segment midpoint.
    {x0 + width / 2, {x0 + slope * width / 2, slope}},

    // Transition segment end, flat segment begin.
    {x0 + width - kEps, {x0 + slope * (width - kEps), slope}},
    {x0 + width, {ceiling, 0}},
    {x0 + width + kEps, {ceiling, 0}},

    // Flat segment interior.
    {x0 + width + 10, {ceiling, 0}},
};
INSTANTIATE_TEST_SUITE_P(TestVectors, EaseOutCallTestNominal,
                         ValuesIn(test_vectors));

}  // namespace nominal

// ----------------------------------------------------------------------------
// Zero x0: linear segment shrinks to a point at origin
// ----------------------------------------------------------------------------

namespace zero_x0 {

constexpr auto x0 = Parameter{0};
constexpr auto width = Parameter{2};
constexpr auto height = Parameter{3};
constexpr auto slope = height / width;

struct EaseOutCallTestZeroX0
    : EaseOutCallTest<TestingTransition<x0, width, height>> {};

TEST_P(EaseOutCallTestZeroX0, Parameterized) { test(); }

constexpr CallTestVector test_vectors[] = {
    // Before transition. This is out of the domain.
    {-kEps, {-kEps, 1}},

    // At transition.
    {0, {0, slope}},

    // Transition segment.
    {kEps, {kEps * slope, slope}},
};
INSTANTIATE_TEST_SUITE_P(TestVectors, EaseOutCallTestZeroX0,
                         ValuesIn(test_vectors));

}  // namespace zero_x0

// ----------------------------------------------------------------------------
// Zero width: transition segment vanishes
// ----------------------------------------------------------------------------

namespace zero_width {

constexpr auto x0 = Parameter{0.5};
constexpr auto ceiling = x0;

struct Transition : DegenerateTransition {
  constexpr auto x0() const noexcept -> Parameter { return zero_width::x0; }
};

struct EaseOutCallTestZeroWidth : EaseOutCallTest<Transition> {};

TEST_P(EaseOutCallTestZeroWidth, Parameterized) { test(); }

constexpr CallTestVector test_vectors[] = {
    // Linear segment.
    {x0 - kEps, {x0 - kEps, 1}},

    // Flat segment begins immediately at x0.
    {x0, {ceiling, 0}},

    // Flat segment.
    {x0 + kEps, {ceiling, 0}},
};
INSTANTIATE_TEST_SUITE_P(TestVectors, EaseOutCallTestZeroWidth,
                         ValuesIn(test_vectors));

}  // namespace zero_width

// ----------------------------------------------------------------------------
// Null Transition: linear segment shrinks to origin, transition vanishes
// ----------------------------------------------------------------------------

namespace null_transition {

constexpr auto ceiling = Parameter{0};

struct Transition : DegenerateTransition {
  constexpr auto x0() const noexcept -> Parameter { return 0; }
};

struct EaseOutCallTestNullTransition : EaseOutCallTest<Transition> {};

TEST_P(EaseOutCallTestNullTransition, Parameterized) { test(); }

constexpr CallTestVector test_vectors[] = {
    // Before what would be either the linear segment or the transition.
    {-kEps, {-kEps, 1}},

    // Flat segment begins immediately at 0.
    {0, {ceiling, 0}},

    // Flat segment.
    {kEps, {ceiling, 0}},
};
INSTANTIATE_TEST_SUITE_P(TestVectors, EaseOutCallTestNullTransition,
                         ValuesIn(test_vectors));

}  // namespace null_transition

}  // namespace

// ============================================================================
// inverse()
// ============================================================================

namespace inverse {
namespace {

struct EaseOutInverseTest : Test {
  static constexpr auto x0 = Parameter{1};
  static constexpr auto width = Parameter{1};
  static constexpr auto height = Parameter{1};
  static constexpr auto ceiling = x0 + height;

  StrictMock<MockTransition> mock_transition;

  using Transition = inverse::Transition<x0, width, height>;

  using Sut = EaseOut<Parameter, Transition, Inverter>;
  Sut sut{Transition{{}, &mock_transition}, inverter};
};

TEST_F(EaseOutInverseTest, LinearSegment) {
  const auto y = x0 / 2;  // Below x0, in linear segment.
  const auto expected = y;

  const auto actual = sut.inverse(y);

  EXPECT_DOUBLE_EQ(expected, actual);
}

TEST_F(EaseOutInverseTest, FlatSegment) {
  const auto y = ceiling + 1;  // Above ceiling, in flat segment.
  const auto expected = x0 + width;

  const auto actual = sut.inverse(y);

  EXPECT_DOUBLE_EQ(expected, actual);
}

TEST_F(EaseOutInverseTest, TransitionSegment) {
  const auto y = (x0 + ceiling) / 2;  // Between x0 and ceiling.
  const auto transition_y = y - x0;   // What we pass to transition_.inverse.
  const auto expected = 17;
  EXPECT_CALL(mock_transition, inverse(transition_y, inverter))
      .WillOnce(Return(expected));

  const auto actual = sut.inverse(y);

  EXPECT_DOUBLE_EQ(expected, actual);
}

}  // namespace
}  // namespace inverse

namespace {

// ============================================================================
// critical_points()
// ============================================================================

struct EaseOutCriticalPointsTest : Test {
  static constexpr auto x0 = Parameter{2};
  static constexpr auto width = Parameter{5};
  static constexpr auto height = Parameter{11};

  using Sut =
      EaseOut<Parameter, TestingTransition<x0, width, height>, Inverter>;
  Sut sut{{}, {}};
};

TEST_F(EaseOutCriticalPointsTest, CriticalPoints) {
  const auto expected = std::array{x0, x0 + width};

  const auto actual = sut.critical_points();

  ASSERT_EQ(expected, actual);
}

/// ============================================================================
// Continuity
// ============================================================================

/*
  See EaseInContinuityTest for an explanation why we're using production parts
  in this unit test, and why it's only coincidentally an integration test.
*/

struct EaseOutContinuityTest : Test {
  using TransitionFunction = transition_functions::Reflected<
      transition_functions::SmootherStepIntegral<Parameter>>;
  using Transition = shaping::Transition<Parameter, TransitionFunction>;
  using Sut = EaseOut<Parameter, Transition, Inverter>;

  static constexpr auto x0 = Parameter{0.45};
  static constexpr auto width = Parameter{2.1};

  static constexpr auto transition = Transition{x0, width};
  static constexpr auto sut = Sut{transition, {}};

  static constexpr auto height = transition.height();
  static constexpr auto ceiling = x0 + height;
};

TEST_F(EaseOutContinuityTest, AtX0) {
  const auto y = sut(Jet{x0, 1.0});

  EXPECT_DOUBLE_EQ(x0, y.a);
  EXPECT_DOUBLE_EQ(1.0, y.v);
}

TEST_F(EaseOutContinuityTest, AtX0PlusWidth) {
  const auto y = sut(Jet{x0 + width, 1.0});

  EXPECT_DOUBLE_EQ(ceiling, y.a);
  EXPECT_DOUBLE_EQ(0.0, y.v);
}
}  // namespace
}  // namespace curves::shaping
