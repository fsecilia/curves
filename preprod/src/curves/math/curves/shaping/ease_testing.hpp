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

using Jet = math::Jet<real_t>;

template <real_t kX0, real_t kWidth, real_t kHeight>
struct TestingTransition {
  constexpr auto x0() const noexcept -> real_t { return kX0; }
  constexpr auto width() const noexcept -> real_t { return kWidth; }
  constexpr auto height() const noexcept -> real_t { return kHeight; }

  template <typename Value>
  constexpr auto operator()(const Value& x) const noexcept -> Value {
    return (x - Value{x0()}) * Value{height() / width()};
  }
};

// Inversion just needs a value to make sure it's not the default. We
using Inverter = int_t;
constexpr auto inverter = Inverter{17};

struct CallTestVector {
  real_t x;
  Jet expected;

  friend auto operator<<(std::ostream& out, const CallTestVector& src)
      -> std::ostream& {
    return out << "{.x = " << src.x << ", .expected = " << src.expected << "}";
  }
};

constexpr auto kEps = 1e-5;

struct DegenerateTransition {
  constexpr auto width() const noexcept -> real_t { return 0; }
  constexpr auto height() const noexcept -> real_t { return 0; }

  auto fail() const -> void { GTEST_FAIL(); }

  template <typename Value>
  auto operator()(const Value&) const -> Value {
    fail();
    return {};
  }
};

namespace inverse {

struct MockTransition {
  MOCK_METHOD(real_t, inverse, (real_t), (const, noexcept));
  virtual ~MockTransition() = default;
};

template <real_t kX0, real_t kWidth, real_t kHeight>
struct Transition : shaping::TestingTransition<kX0, kWidth, kHeight> {
  auto inverse(real_t y) const noexcept -> real_t {
    return mock_transition->inverse(y);
  }

  MockTransition* mock_transition = nullptr;
};

}  // namespace inverse
}  // namespace curves::shaping
