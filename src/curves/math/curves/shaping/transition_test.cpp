// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2026 Frank Secilia
*/

#include "transition.hpp"
#include <curves/testing/test.hpp>
#include <curves/math/jet.hpp>
#include <gmock/gmock.h>

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
// Common Fixture
// ============================================================================

struct TransitionTest : Test {
  struct TransitionFunction {
    constexpr auto at_1() const noexcept -> Parameter { return m; }

    template <typename Value>
    constexpr auto operator()(const Value& t) const noexcept -> Value {
      return m * t;
    }
  };

  struct MockInverter {
    MOCK_METHOD(double, call, (const TransitionFunction&, double y),
                (const, noexcept));
    virtual ~MockInverter() = default;
  };
  StrictMock<MockInverter> mock_inverter;

  struct Inverter {
    auto operator()(const TransitionFunction& transition_function,
                    double y) const noexcept -> double {
      return mock->call(transition_function, y);
    }

    MockInverter* mock = nullptr;
  };

  using Sut = Transition<Parameter, TransitionFunction, Inverter>;
};

// ============================================================================
// Valid Instance Tests
// ============================================================================

/*
  These test against a valid instance, compared to a death test, which tests
  against an invalid instance.
*/
struct TransitionTestValidInstance : TransitionTest {
  Sut sut{x0, width, TransitionFunction{}, Inverter{&mock_inverter}};
};

// ----------------------------------------------------------------------------
// Properties
// ----------------------------------------------------------------------------

struct TransitionTestProperties : TransitionTestValidInstance {};

TEST_F(TransitionTestProperties, x0) { EXPECT_DOUBLE_EQ(x0, sut.x0()); }

TEST_F(TransitionTestProperties, Width) {
  EXPECT_DOUBLE_EQ(width, sut.width());
}

TEST_F(TransitionTestProperties, Height) {
  EXPECT_DOUBLE_EQ(m * width, sut.height());
}

TEST_F(TransitionTestProperties, Inverter) {
  EXPECT_EQ(&mock_inverter, sut.inverter().mock);
}

// ----------------------------------------------------------------------------
// Parameterized Test
// ----------------------------------------------------------------------------

struct TransitionParameterizedTest : TransitionTestValidInstance,
                                     WithParamInterface<Parameter> {};

TEST_P(TransitionParameterizedTest, Evaluate) {
  const auto x = Jet{GetParam(), 1.0};
  const auto y_expected = m * (x - x0);

  const auto y_actual = sut(x);

  EXPECT_NEAR(y_expected.a, y_actual.a, 1e-10);
  EXPECT_NEAR(y_expected.v, y_actual.v, 1e-10);
}

TEST_P(TransitionParameterizedTest, Inverse) {
  const auto y = GetParam();
  const auto y_normalized = y / width;
  const auto x_normalized = 2 * y;
  const auto x_expected = x_normalized * width + x0;

  EXPECT_CALL(mock_inverter, call(Ref(sut.transition_function()),
                                  DoubleNear(y_normalized, 1e-10)))
      .WillOnce(Return(x_normalized));

  const auto x_actual = sut.inverse(y);

  EXPECT_DOUBLE_EQ(x_expected, x_actual);
}

INSTANTIATE_TEST_SUITE_P(TransitionTest, TransitionParameterizedTest,
                         ValuesIn(test_vectors));

// ----------------------------------------------------------------------------
// Death Tests
// ----------------------------------------------------------------------------

#if !defined NDEBUG

struct TransitionDeathTest : TransitionTest {
  Sut sut{0, 0, {}, {}};
};

TEST_F(TransitionDeathTest, Evaluate) {
  EXPECT_DEBUG_DEATH(sut(0.0), "domain error");
}

TEST_F(TransitionDeathTest, Invert) {
  EXPECT_DEBUG_DEATH(sut.inverse(0.0), "domain error");
}

#endif

}  // namespace
}  // namespace curves::shaping
