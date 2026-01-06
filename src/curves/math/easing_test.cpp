// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2026 Frank Secilia
*/

#include "easing.hpp"
#include <curves/testing/test.hpp>
#include <ostream>

namespace curves::math {
namespace {

// ============================================================================
// Test Fixture
// ============================================================================

struct EasingFunctionTest : Test {
  static constexpr auto kEpsilon = 1e-15;

  using Sut = EasingFunction;
  Sut sut;
};

// ----------------------------------------------------------------------------
// Global Properties
// ----------------------------------------------------------------------------

TEST_F(EasingFunctionTest, Monotonic) {
  auto prev = -1.0;
  for (auto t = 0.0; t <= 1.0; t += 0.05) {
    auto current = sut(t);
    EXPECT_GT(current, prev);
    prev = current;
  }
}

// ----------------------------------------------------------------------------
// Specific Points
// ----------------------------------------------------------------------------

struct EasingFunctionTestVector {
  Jet<double> t;
  Jet<double> y;

  friend auto operator<<(std::ostream& out, const EasingFunctionTestVector& src)
      -> std::ostream& {
    return out << "{.t = " << src.t << ", .y = " << src.y << "}";
  }
};

struct EasingFunctionParameterizedTest
    : EasingFunctionTest,
      WithParamInterface<EasingFunctionTestVector> {};

TEST_P(EasingFunctionParameterizedTest, SpecificPoints) {
  const auto expected_y = GetParam().y;

  const auto actual_y = sut(GetParam().t);

  EXPECT_NEAR(expected_y.a, actual_y.a, kEpsilon);
  EXPECT_NEAR(expected_y.v, actual_y.v, kEpsilon);
}

const EasingFunctionTestVector easing_function_test_vectors[] = {
    {{0.0, 1.0}, {0.0, 0.0}},
    {{0.5, 1.0}, {0.078125, 0.5}},
    {{1.0, 1.0}, {0.5, 1.0}},
};
INSTANTIATE_TEST_SUITE_P(EasingFunctionTest, EasingFunctionParameterizedTest,
                         ValuesIn(easing_function_test_vectors));

}  // namespace
}  // namespace curves::math
