// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2026 Frank Secilia
*/

#include "ease_in.hpp"
#include <curves/testing/test.hpp>
#include <curves/math/jet.hpp>

namespace curves::shaping {
namespace {

using Parameter = double;
using Jet = math::Jet<Parameter>;

// ============================================================================
// operator()
// ============================================================================

// ----------------------------------------------------------------------------
// Test Double
// ----------------------------------------------------------------------------

template <Parameter kX0, Parameter kWidth, Parameter kHeight>
struct Transition {
  constexpr auto x0() const noexcept -> Parameter { return kX0; }
  constexpr auto width() const noexcept -> Parameter { return kWidth; }
  constexpr auto height() const noexcept -> Parameter { return kHeight; }

  template <typename Value>
  constexpr auto operator()(const Value& x) const noexcept -> Value {
    return (x - Value{x0()}) * Value{height() / width()};
  }
};

// ----------------------------------------------------------------------------
// Fixture
// ----------------------------------------------------------------------------

struct CallTestVector {
  Parameter x;
  Jet expected;

  friend auto operator<<(std::ostream& out, const CallTestVector& src)
      -> std::ostream& {
    return out << "{.x = " << src.x << ", .expected = " << src.expected << "}";
  }
};

template <typename Transition>
struct EaseInCallTest : TestWithParam<CallTestVector> {
  using Sut = EaseIn<Parameter, Transition>;
  Sut sut{{}};

  auto test() const -> void {
    const auto x = Jet{GetParam().x, 1.0};
    const auto expected = GetParam().expected;

    const auto actual = sut(x);

    EXPECT_NEAR(expected.a, actual.a, 1e-10);
    EXPECT_NEAR(expected.v, actual.v, 1e-10);
  }
};

constexpr auto kEps = 1e-5;

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

struct EaseInCallTestNominal : EaseInCallTest<Transition<x0, width, height>> {};

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

struct EaseInCallTestZeroX0 : EaseInCallTest<Transition<x0, width, height>> {};

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

struct Transition {
  constexpr auto x0() const noexcept -> Parameter { return zero_width::x0; }
  constexpr auto width() const noexcept -> Parameter { return 0; }
  constexpr auto height() const noexcept -> Parameter { return 0; }

  auto fail() const -> void { GTEST_FAIL(); }

  template <typename Value>
  auto operator()(const Value&) const -> Value {
    fail();
    return {};
  }
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

struct Transition {
  constexpr auto x0() const noexcept -> Parameter { return 0; }
  constexpr auto width() const noexcept -> Parameter { return 0; }
  constexpr auto height() const noexcept -> Parameter { return 0; }

  auto fail() const -> void { GTEST_FAIL(); }

  template <typename Value>
  auto operator()(const Value&) const -> Value {
    fail();
    return {};
  }
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
}  // namespace curves::shaping
