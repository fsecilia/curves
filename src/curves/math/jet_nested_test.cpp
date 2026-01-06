// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Tests for nested jets (Jet<Jet<Element>>).

  These tests verify that autodiff composes correctly for computing
  second derivatives via Jet<Jet<double>>.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#include "jet.hpp"
#include <curves/testing/test.hpp>
#include <cmath>

namespace curves::math {
namespace {

// ============================================================================
// Test Fixture
// ============================================================================

struct NestedJetTest : Test {
  using E = double;
  using J1 = Jet<E>;
  using J2 = Jet<J1>;

  //! Create a doubly-seeded jet for computing f(a), f'(a), and f''(a).
  //! Structure: {{a, 1}, {1, 0}}
  static constexpr auto seed(E a) -> J2 { return {{a, 1.0}, {1.0, 0.0}}; }

  //! Extract f(a) from nested jet result.
  static constexpr auto value(const J2& x) -> E { return primal(primal(x)); }

  //! Extract f'(a) from nested jet result.
  static constexpr auto first_derivative(const J2& x) -> E {
    return derivative(primal(x));
  }

  //! Extract f''(a) from nested jet result.
  static constexpr auto second_derivative(const J2& x) -> E {
    return derivative(derivative(x));
  }

  static constexpr E kEps = 1e-10;
};

// ============================================================================
// Arithmetic
// ============================================================================

struct NestedJetTestArithmetic : NestedJetTest {};

TEST_F(NestedJetTestArithmetic, Identity) {
  // f(x) = x
  // f'(x) = 1
  // f''(x) = 0
  const auto x = seed(3.0);

  EXPECT_DOUBLE_EQ(value(x), 3.0);
  EXPECT_DOUBLE_EQ(first_derivative(x), 1.0);
  EXPECT_DOUBLE_EQ(second_derivative(x), 0.0);
}

TEST_F(NestedJetTestArithmetic, Square) {
  // f(x) = x^2
  // f'(x) = 2x
  // f''(x) = 2
  const auto x = seed(3.0);

  const auto result = x * x;

  EXPECT_DOUBLE_EQ(value(result), 9.0);
  EXPECT_DOUBLE_EQ(first_derivative(result), 6.0);
  EXPECT_DOUBLE_EQ(second_derivative(result), 2.0);
}

TEST_F(NestedJetTestArithmetic, Cube) {
  // f(x) = x^3
  // f'(x) = 3x^2
  // f''(x) = 6x
  const auto x = seed(2.0);

  const auto result = x * x * x;

  EXPECT_DOUBLE_EQ(value(result), 8.0);
  EXPECT_DOUBLE_EQ(first_derivative(result), 12.0);
  EXPECT_DOUBLE_EQ(second_derivative(result), 12.0);
}

TEST_F(NestedJetTestArithmetic, Quartic) {
  // f(x) = x^4
  // f'(x) = 4x^3
  // f''(x) = 12x^2
  const auto x = seed(2.0);
  const auto x2 = x * x;

  const auto result = x2 * x2;

  EXPECT_DOUBLE_EQ(value(result), 16.0);
  EXPECT_DOUBLE_EQ(first_derivative(result), 32.0);
  EXPECT_DOUBLE_EQ(second_derivative(result), 48.0);
}

TEST_F(NestedJetTestArithmetic, Reciprocal) {
  // f(x) = 1/x
  // f'(x) = -1/x^2
  // f''(x) = 2/x^3
  const auto x = seed(2.0);

  const auto result = J2{1.0} / x;

  EXPECT_DOUBLE_EQ(value(result), 0.5);
  EXPECT_DOUBLE_EQ(first_derivative(result), -0.25);
  EXPECT_DOUBLE_EQ(second_derivative(result), 0.25);
}

TEST_F(NestedJetTestArithmetic, LinearCombination) {
  // f(x) = 3x^2 + 2x + 1
  // f'(x) = 6x + 2
  // f''(x) = 6
  const auto x = seed(2.0);

  const auto result = J2{3.0} * x * x + J2{2.0} * x + J2{1.0};

  EXPECT_DOUBLE_EQ(value(result), 17.0);
  EXPECT_DOUBLE_EQ(first_derivative(result), 14.0);
  EXPECT_DOUBLE_EQ(second_derivative(result), 6.0);
}

// ============================================================================
// Transcendental Functions
// ============================================================================

struct NestedJetTestTranscendental : NestedJetTest {};

TEST_F(NestedJetTestTranscendental, Exp) {
  // f(x) = e^x
  // f'(x) = e^x
  // f''(x) = e^x
  const auto x = seed(1.0);

  const auto result = exp(x);

  EXPECT_NEAR(value(result), M_E, kEps);
  EXPECT_NEAR(first_derivative(result), M_E, kEps);
  EXPECT_NEAR(second_derivative(result), M_E, kEps);
}

TEST_F(NestedJetTestTranscendental, Log) {
  // f(x) = ln(x)
  // f'(x) = 1/x
  // f''(x) = -1/x^2
  const auto x = seed(2.0);

  const auto result = log(x);

  EXPECT_NEAR(value(result), std::log(2.0), kEps);
  EXPECT_NEAR(first_derivative(result), 0.5, kEps);
  EXPECT_NEAR(second_derivative(result), -0.25, kEps);
}

TEST_F(NestedJetTestTranscendental, Sqrt) {
  // f(x) = sqrt(x)
  // f'(x) = 1/(2*sqrt(x))
  // f''(x) = -1/(4*x^(3/2))
  const auto x = seed(4.0);

  const auto result = sqrt(x);

  EXPECT_NEAR(value(result), 2.0, kEps);
  EXPECT_NEAR(first_derivative(result), 0.25, kEps);
  EXPECT_NEAR(second_derivative(result), -1.0 / 32.0, kEps);
}

TEST_F(NestedJetTestTranscendental, Tanh) {
  // f(x) = tanh(x)
  // f'(x) = sech^2(x) = 1 - tanh^2(x)
  // f''(x) = -2*tanh(x)*sech^2(x)
  const auto x = seed(1.0);

  const auto result = tanh(x);

  const auto t = std::tanh(1.0);
  const auto sech2 = 1.0 - t * t;

  EXPECT_NEAR(value(result), t, kEps);
  EXPECT_NEAR(first_derivative(result), sech2, kEps);
  EXPECT_NEAR(second_derivative(result), -2.0 * t * sech2, kEps);
}

TEST_F(NestedJetTestTranscendental, TanhAtZero) {
  // f(0) = 0
  // f'(0) = 1
  // f''(0) = 0
  const auto x = seed(0.0);

  const auto result = tanh(x);

  EXPECT_NEAR(value(result), 0.0, kEps);
  EXPECT_NEAR(first_derivative(result), 1.0, kEps);
  EXPECT_NEAR(second_derivative(result), 0.0, kEps);
}

// ============================================================================
// Power Functions
// ============================================================================

struct NestedJetTestPow : NestedJetTest {};

TEST_F(NestedJetTestPow, JetElement) {
  // f(x) = x^3 (using pow)
  // f'(x) = 3x^2
  // f''(x) = 6x
  const auto x = seed(2.0);

  const auto result = pow(x, 3.0);

  EXPECT_NEAR(value(result), 8.0, kEps);
  EXPECT_NEAR(first_derivative(result), 12.0, kEps);
  EXPECT_NEAR(second_derivative(result), 12.0, kEps);
}

TEST_F(NestedJetTestPow, JetElementFractional) {
  // f(x) = x^1.5
  // f'(x) = 1.5 * x^0.5
  // f''(x) = 0.75 * x^(-0.5)
  const auto x = seed(4.0);

  const auto result = pow(x, 1.5);

  EXPECT_NEAR(value(result), 8.0, kEps);                // 4^1.5 = 8
  EXPECT_NEAR(first_derivative(result), 3.0, kEps);     // 1.5 * 4^0.5 = 3
  EXPECT_NEAR(second_derivative(result), 0.375, kEps);  // 0.75 * 4^(-0.5)
}

TEST_F(NestedJetTestPow, ElementJet) {
  // f(x) = 2^x
  // f'(x) = ln(2) * 2^x
  // f''(x) = ln(2)^2 * 2^x
  const auto x = seed(3.0);

  const auto result = pow(2.0, x);

  const auto ln2 = std::log(2.0);

  EXPECT_NEAR(value(result), 8.0, kEps);
  EXPECT_NEAR(first_derivative(result), ln2 * 8.0, kEps);
  EXPECT_NEAR(second_derivative(result), ln2 * ln2 * 8.0, kEps);
}

TEST_F(NestedJetTestPow, JetJet) {
  // f(x) = x^x
  // f'(x) = x^x * (ln(x) + 1)
  // f''(x) = x^x * ((ln(x) + 1)^2 + 1/x)
  const auto x = seed(2.0);

  const auto result = pow(x, x);

  const auto ln2 = std::log(2.0);
  const auto f = 4.0;               // 2^2
  const auto df = f * (ln2 + 1.0);  // 4 * (ln(2) + 1)
  const auto ddf = f * ((ln2 + 1.0) * (ln2 + 1.0) + 0.5);

  EXPECT_NEAR(value(result), f, kEps);
  EXPECT_NEAR(first_derivative(result), df, kEps);
  EXPECT_NEAR(second_derivative(result), ddf, kEps);
}

// ============================================================================
// Composition
// ============================================================================

struct NestedJetTestComposition : NestedJetTest {};

TEST_F(NestedJetTestComposition, ExpOfSquare) {
  // f(x) = e^(x^2)
  // f'(x) = 2x * e^(x^2)
  // f''(x) = 2 * e^(x^2) + 4x^2 * e^(x^2) = (2 + 4x^2) * e^(x^2)
  const auto x = seed(1.0);

  const auto result = exp(x * x);

  const auto e1 = M_E;  // e^1

  EXPECT_NEAR(value(result), e1, kEps);
  EXPECT_NEAR(first_derivative(result), 2.0 * e1, kEps);
  EXPECT_NEAR(second_derivative(result), 6.0 * e1, kEps);  // (2 + 4) * e
}

TEST_F(NestedJetTestComposition, LogOfSquare) {
  // f(x) = ln(x^2) = 2*ln(x)
  // f'(x) = 2/x
  // f''(x) = -2/x^2
  const auto x = seed(3.0);

  const auto result = log(x * x);

  EXPECT_NEAR(value(result), 2.0 * std::log(3.0), kEps);
  EXPECT_NEAR(first_derivative(result), 2.0 / 3.0, kEps);
  EXPECT_NEAR(second_derivative(result), -2.0 / 9.0, kEps);
}

TEST_F(NestedJetTestComposition, SqrtOfQuadratic) {
  // f(x) = sqrt(x^2 + 1)
  // f'(x) = x / sqrt(x^2 + 1)
  // f''(x) = 1 / (x^2 + 1)^(3/2)
  const auto x = seed(2.0);

  const auto result = sqrt(x * x + J2{1.0});

  const auto r = std::sqrt(5.0);

  EXPECT_NEAR(value(result), r, kEps);
  EXPECT_NEAR(first_derivative(result), 2.0 / r, kEps);
  EXPECT_NEAR(second_derivative(result), 1.0 / (5.0 * r), kEps);
}

TEST_F(NestedJetTestComposition, TanhOfExp) {
  // f(x) = tanh(e^x)
  // f'(x) = sech^2(e^x) * e^x
  // f''(x) = e^x * sech^2(e^x) * (1 - 2*tanh(e^x)*e^x)
  const auto x = seed(0.0);  // At x=0: e^x = 1

  const auto result = tanh(exp(x));

  const auto t1 = std::tanh(1.0);
  const auto sech2_1 = 1.0 - t1 * t1;

  // f(0) = tanh(1)
  // f'(0) = sech^2(1) * 1 = sech^2(1)
  // f''(0) = sech^2(1) * (1 - 2*tanh(1)*1)
  EXPECT_NEAR(value(result), t1, kEps);
  EXPECT_NEAR(first_derivative(result), sech2_1, kEps);
  EXPECT_NEAR(second_derivative(result), sech2_1 * (1.0 - 2.0 * t1), kEps);
}

// ============================================================================
// Symmetry Verification
// ============================================================================

/*!
  For smooth functions, the two first derivative components should match:
  derivative(primal(result)) == primal(derivative(result))
  This is Schwarz's theorem, the equality of mixed partials.
*/
struct NestedJetTestMixedPartialSymmetry : NestedJetTest {
  const J2 x = seed(2.0);
};

TEST_F(NestedJetTestMixedPartialSymmetry, Cube) {
  const auto f = x * x * x;  // x^3

  EXPECT_DOUBLE_EQ(derivative(primal(f)), primal(derivative(f)));
}

TEST_F(NestedJetTestMixedPartialSymmetry, Exp) {
  const auto f = exp(x);

  EXPECT_NEAR(derivative(primal(f)), primal(derivative(f)), kEps);
}

TEST_F(NestedJetTestMixedPartialSymmetry, Log) {
  const auto f = log(x);

  EXPECT_NEAR(derivative(primal(f)), primal(derivative(f)), kEps);
}

TEST_F(NestedJetTestMixedPartialSymmetry, Sqrt) {
  const auto f = sqrt(x);

  EXPECT_NEAR(derivative(primal(f)), primal(derivative(f)), kEps);
}

// ============================================================================
// Edge Cases
// ============================================================================

struct NestedJetTestEdgeCases : NestedJetTest {};

TEST_F(NestedJetTestEdgeCases, ConstantFunction) {
  // f(x) = 5
  // f'(x) = 0
  // f''(x) = 0
  const auto x = seed(3.0);

  // Adding zero * x to ensure it goes through the machinery
  const auto result = J2{5.0} + J2{0.0} * x;

  EXPECT_DOUBLE_EQ(value(result), 5.0);
  EXPECT_DOUBLE_EQ(first_derivative(result), 0.0);
  EXPECT_DOUBLE_EQ(second_derivative(result), 0.0);
}

TEST_F(NestedJetTestEdgeCases, LinearFunction) {
  // f(x) = 3x + 2
  // f'(x) = 3
  // f''(x) = 0
  const auto x = seed(5.0);

  const auto result = J2{3.0} * x + J2{2.0};

  EXPECT_DOUBLE_EQ(value(result), 17.0);
  EXPECT_DOUBLE_EQ(first_derivative(result), 3.0);
  EXPECT_DOUBLE_EQ(second_derivative(result), 0.0);
}

TEST_F(NestedJetTestEdgeCases, Hypot) {
  // f(x) = hypot(x, 3) = sqrt(x^2 + 9)
  // f'(x) = x / sqrt(x^2 + 9)
  // f''(x) = 9 / (x^2 + 9)^(3/2)
  const auto x = seed(4.0);
  const auto three = J2{3.0};

  const auto result = hypot(x, three);

  const auto r = 5.0;  // sqrt(16 + 9)

  EXPECT_DOUBLE_EQ(value(result), r);
  EXPECT_DOUBLE_EQ(first_derivative(result), 4.0 / 5.0);
  EXPECT_NEAR(second_derivative(result), 9.0 / 125.0, kEps);
}

// ============================================================================
// Type Promotion
// ============================================================================

struct NestedJetTestTypePromotion : NestedJetTest {};

TEST_F(NestedJetTestTypePromotion, J1PromotedToJ2Addition) {
  // Explicit construction required (per Jet API)
  const auto j1 = J1{3.0, 2.0};
  const auto j2 = J2{{5.0, 1.0}, {1.0, 0.0}};

  const auto result = J2{j1} + j2;

  EXPECT_DOUBLE_EQ(primal(primal(result)), 8.0);
}

TEST_F(NestedJetTestTypePromotion, J1PromotedToJ2Multiplication) {
  // Constant J1 as coefficient in J2 expression
  const auto coeff = J1{2.0, 0.0};  // Just a constant "2" as J1
  const auto x = J2{{3.0, 1.0}, {1.0, 0.0}};

  const auto result = J2{coeff} * x;

  // 2 * x at x=3, so primal = 6, first deriv = 2, second = 0
  EXPECT_DOUBLE_EQ(primal(primal(result)), 6.0);
  EXPECT_DOUBLE_EQ(derivative(primal(result)), 2.0);
}

TEST_F(NestedJetTestTypePromotion, NestedPolynomialWithScalars) {
  // f(x) = 3x^2 + 2x + 1 using J2
  // f(2) = 17, f'(2) = 14, f''(2) = 6
  const auto x = seed(2.0);

  const auto result = 3.0 * x * x + 2.0 * x + J1{1.0};

  EXPECT_DOUBLE_EQ(value(result), 17.0);
  EXPECT_DOUBLE_EQ(first_derivative(result), 14.0);
  EXPECT_DOUBLE_EQ(second_derivative(result), 6.0);
}

}  // namespace
}  // namespace curves::math
