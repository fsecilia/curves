// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Common facilities for testing ease functions.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <curves/testing/test.hpp>
#include <curves/math/jet.hpp>
#include <gmock/gmock.h>

namespace curves::shaping {

using Parameter = double;
using Jet = math::Jet<Parameter>;

template <Parameter kX0, Parameter kWidth, Parameter kHeight>
struct TestingTransition {
  constexpr auto x0() const noexcept -> Parameter { return kX0; }
  constexpr auto width() const noexcept -> Parameter { return kWidth; }
  constexpr auto height() const noexcept -> Parameter { return kHeight; }

  template <typename Value>
  constexpr auto operator()(const Value& x) const noexcept -> Value {
    return (x - Value{x0()}) * Value{height() / width()};
  }
};

// Inversion just needs a value to make sure it's not the default.
using Inverter = int_t;
constexpr auto inverter = Inverter{17};

struct CallTestVector {
  Parameter x;
  Jet expected;

  friend auto operator<<(std::ostream& out, const CallTestVector& src)
      -> std::ostream& {
    return out << "{.x = " << src.x << ", .expected = " << src.expected << "}";
  }
};

constexpr auto kEps = 1e-5;

struct DegenerateTransition {
  constexpr auto width() const noexcept -> Parameter { return 0; }
  constexpr auto height() const noexcept -> Parameter { return 0; }

  auto fail() const -> void { GTEST_FAIL(); }

  template <typename Value>
  auto operator()(const Value&) const -> Value {
    fail();
    return {};
  }
};

namespace inverse {

struct MockTransition {
  MOCK_METHOD(Parameter, inverse, (Parameter, const Inverter& inverter),
              (const, noexcept));
  virtual ~MockTransition() = default;
};

template <Parameter kX0, Parameter kWidth, Parameter kHeight>
struct Transition : shaping::TestingTransition<kX0, kWidth, kHeight> {
  auto inverse(Parameter y, const Inverter& inverter) const noexcept
      -> Parameter {
    return mock_transition->inverse(y, inverter);
  }

  MockTransition* mock_transition = nullptr;
};

}  // namespace inverse
}  // namespace curves::shaping
