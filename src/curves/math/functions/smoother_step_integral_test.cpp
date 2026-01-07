// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2026 Frank Secilia
*/

#include "smoother_step_integral.hpp"
#include <curves/testing/test.hpp>
#include <ostream>

namespace curves::math {
namespace {

// ============================================================================
// Test Fixture
// ============================================================================

struct SmootherStepIntegralTest : Test {
  using Parameter = double;

  static constexpr auto kEpsilon = 1e-15;

  using Sut = SmootherStepIntegral<Parameter>;
  static constexpr Sut sut{};
};

// ----------------------------------------------------------------------------
// Global Properties
// ----------------------------------------------------------------------------

TEST_F(SmootherStepIntegralTest, Monotonic) {
  auto prev = -1.0;
  for (auto t = 0.0; t <= 1.0; t += 0.05) {
    const auto current = sut(t);
    EXPECT_GT(current, prev);
    prev = current;
  }
}

TEST_F(SmootherStepIntegralTest, At_1) { EXPECT_DOUBLE_EQ(0.5, sut.at_1()); }

// ----------------------------------------------------------------------------
// Specific Points
// ----------------------------------------------------------------------------

struct SmootherStepIntegralTestVector {
  Jet<double> t;
  Jet<double> y;

  friend auto operator<<(std::ostream& out,
                         const SmootherStepIntegralTestVector& src)
      -> std::ostream& {
    return out << "{.t = " << src.t << ", .y = " << src.y << "}";
  }
};

struct SmootherStepIntegralParameterizedTest
    : SmootherStepIntegralTest,
      WithParamInterface<SmootherStepIntegralTestVector> {};

TEST_P(SmootherStepIntegralParameterizedTest, SpecificPoints) {
  const auto expected_y = GetParam().y;

  const auto actual_y = sut(GetParam().t);

  EXPECT_NEAR(expected_y.a, actual_y.a, kEpsilon);
  EXPECT_NEAR(expected_y.v, actual_y.v, kEpsilon);
}

const SmootherStepIntegralTestVector easing_function_test_vectors[] = {
    {{0.0, 1.0}, {0.0, 0.0}},
    {{0.5, 1.0}, {0.078125, 0.5}},
    {{1.0, 1.0}, {0.5, 1.0}},
};
INSTANTIATE_TEST_SUITE_P(SmootherStepIntegralTest,
                         SmootherStepIntegralParameterizedTest,
                         ValuesIn(easing_function_test_vectors));

}  // namespace
}  // namespace curves::math
