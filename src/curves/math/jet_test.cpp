// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Tests for autodiffing jet implementation.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#include "jet.hpp"
#include <curves/testing/test.hpp>
#include <cmath>
#include <limits>
#include <ostream>
#include <sstream>

namespace curves::math {
namespace {

// ============================================================================
// Test Fixture
// ============================================================================

struct JetTest : Test {
  using E = double;
  using J = Jet<E>;

  //! Create a seed jet for derivative verification: f(seed(a)).v == f'(a)
  static constexpr auto seed(E a) -> J { return {a, 1.0}; }

  // Common constants
  static constexpr E a = 42.0;  // arbitrary
  static constexpr E v = 31.0;  // arbitrary
  static constexpr J x{a, v};

  // Edge values
  const E inf_e = std::numeric_limits<E>::infinity();
  const E nan_e = std::numeric_limits<E>::quiet_NaN();

  // Tolerances
  static constexpr E kEps = 1e-12;
};

// ============================================================================
// Construction
// ============================================================================

struct JetTestConstruction : JetTest {};

TEST_F(JetTestConstruction, Default) {
  constexpr auto j = J();

  EXPECT_EQ(primal(j), 0.0);
  EXPECT_EQ(derivative(j), 0.0);
}

TEST_F(JetTestConstruction, Element) {
  constexpr auto j = J{42.0};

  EXPECT_EQ(primal(j), 42.0);
  EXPECT_EQ(derivative(j), 0.0);
}

TEST_F(JetTestConstruction, Pair) {
  constexpr auto j = J{3.0, 4.0};

  EXPECT_EQ(primal(j), 3.0);
  EXPECT_EQ(derivative(j), 4.0);
}

// ============================================================================
// Conversion
// ============================================================================

struct JetTestConversion : JetTest {
  static constexpr int_t ai = 7;
  static constexpr int_t vi = 11;
};

TEST_F(JetTestConversion, Constructor) {
  constexpr auto ji = Jet{ai, vi};

  constexpr auto jd = J{ji};

  EXPECT_DOUBLE_EQ(primal(jd), static_cast<E>(ai));
  EXPECT_DOUBLE_EQ(derivative(jd), static_cast<E>(vi));
}

TEST_F(JetTestConversion, Assignment) {
  constexpr auto ji = Jet{ai, vi};
  auto jd = J{};

  jd = ji;

  EXPECT_DOUBLE_EQ(primal(jd), static_cast<E>(ai));
  EXPECT_DOUBLE_EQ(derivative(jd), static_cast<E>(vi));
}

TEST_F(JetTestConversion, ToBoolTrue) {
  EXPECT_TRUE(static_cast<bool>(J{1.0, 0.0}));
  EXPECT_TRUE(static_cast<bool>(J{-1.0, 0.0}));
  EXPECT_TRUE(static_cast<bool>(J{0.001, 0.0}));

  // Derivative is ignored - nonzero primal means true.
  EXPECT_TRUE(static_cast<bool>(J{1.0, 999.0}));
}

TEST_F(JetTestConversion, ToBoolFalse) {
  EXPECT_FALSE(static_cast<bool>(J{0.0, 0.0}));

  // Derivative is ignored - zero primal means false.
  EXPECT_FALSE(static_cast<bool>(J{0.0, 999.0}));
}

// ============================================================================
// Scalar Fallback
// ============================================================================

struct JetTestScalarFallback : JetTest {};

TEST_F(JetTestScalarFallback, Primal) {
  EXPECT_EQ(primal(a), a);
  EXPECT_EQ(primal(-a), -a);
  EXPECT_EQ(primal(v), v);
}

TEST_F(JetTestScalarFallback, Derivative) {
  EXPECT_EQ(derivative(a), 0.0);
  EXPECT_EQ(derivative(-a), 0.0);
  EXPECT_EQ(derivative(v), 0.0);
}

// ============================================================================
// Unary Arithmetic
// ============================================================================

struct JetTestUnaryArithmetic : JetTest {};

TEST_F(JetTestUnaryArithmetic, Plus) {
  constexpr auto result = +x;

  EXPECT_EQ(primal(result), primal(x));
  EXPECT_EQ(derivative(result), derivative(x));
}

TEST_F(JetTestUnaryArithmetic, Minus) {
  constexpr auto result = -x;

  EXPECT_EQ(primal(result), -primal(x));
  EXPECT_EQ(derivative(result), -derivative(x));
}

// ============================================================================
// Element Arithmetic
// ============================================================================

// ----------------------------------------------------------------------------
// Addition
// ----------------------------------------------------------------------------

struct JetTestElementAddition : JetTest {};

TEST_F(JetTestElementAddition, JetPlusElement) {
  const auto result = J{3.0, 5.0} + 2.0;

  EXPECT_DOUBLE_EQ(primal(result), 5.0);
  EXPECT_DOUBLE_EQ(derivative(result), 5.0);
}

TEST_F(JetTestElementAddition, ElementPlusJet) {
  const auto result = 2.0 + J{3.0, 5.0};

  EXPECT_DOUBLE_EQ(primal(result), 5.0);
  EXPECT_DOUBLE_EQ(derivative(result), 5.0);
}

// ----------------------------------------------------------------------------
// Subtraction
// ----------------------------------------------------------------------------

struct JetTestElementSubtraction : JetTest {};

TEST_F(JetTestElementSubtraction, JetMinusElement) {
  const auto result = J{3.0, 5.0} - 2.0;

  EXPECT_DOUBLE_EQ(primal(result), 1.0);
  EXPECT_DOUBLE_EQ(derivative(result), 5.0);
}

TEST_F(JetTestElementSubtraction, ElementMinusJet) {
  const auto result = 10.0 - J{3.0, 5.0};

  EXPECT_DOUBLE_EQ(primal(result), 7.0);
  EXPECT_DOUBLE_EQ(derivative(result), -5.0);
}

// ----------------------------------------------------------------------------
// Multiplication
// ----------------------------------------------------------------------------

struct JetElementMultiplicationVector {
  using E = JetTest::E;
  using J = JetTest::J;

  J jet;
  E scalar;
  J expected;

  friend auto operator<<(std::ostream& os,
                         const JetElementMultiplicationVector& c)
      -> std::ostream& {
    return os << c.jet << " * " << c.scalar << " = " << c.expected;
  }
};

struct JetTestElementMultiplication
    : JetTest,
      WithParamInterface<JetElementMultiplicationVector> {};

TEST_P(JetTestElementMultiplication, JetTimesElement) {
  const auto [jet, scalar, expected] = GetParam();

  const auto result = jet * scalar;

  EXPECT_DOUBLE_EQ(primal(result), primal(expected));
  EXPECT_DOUBLE_EQ(derivative(result), derivative(expected));
}

TEST_P(JetTestElementMultiplication, ElementTimesJet) {
  const auto [jet, scalar, expected] = GetParam();

  const auto result = scalar * jet;

  EXPECT_DOUBLE_EQ(primal(result), primal(expected));
  EXPECT_DOUBLE_EQ(derivative(result), derivative(expected));
}

TEST_P(JetTestElementMultiplication, JetTimesElementInPlace) {
  auto [jet, scalar, expected] = GetParam();

  jet *= scalar;

  EXPECT_DOUBLE_EQ(primal(jet), primal(expected));
  EXPECT_DOUBLE_EQ(derivative(jet), derivative(expected));
}

const JetElementMultiplicationVector element_multiplication_vectors[] = {
    {{3.0, 2.0}, 5.0, {15.0, 10.0}}, {{3.0, 2.0}, 0.0, {0.0, 0.0}},
    {{3.0, 2.0}, 1.0, {3.0, 2.0}},   {{3.0, 2.0}, -1.0, {-3.0, -2.0}},
    {{0.0, 0.0}, 5.0, {0.0, 0.0}},
};
INSTANTIATE_TEST_SUITE_P(ElementMultiplication, JetTestElementMultiplication,
                         ValuesIn(element_multiplication_vectors));

// ----------------------------------------------------------------------------
// Division
// ----------------------------------------------------------------------------

struct JetElementDivisionVector {
  using E = JetTest::E;
  using J = JetTest::J;

  J jet;
  E scalar;
  J expected;

  friend auto operator<<(std::ostream& os, const JetElementDivisionVector& c)
      -> std::ostream& {
    return os << c.jet << " / " << c.scalar << " = " << c.expected;
  }
};

struct JetTestElementDivision : JetTest,
                                WithParamInterface<JetElementDivisionVector> {};

TEST_P(JetTestElementDivision, JetDividedByElement) {
  const auto [jet, scalar, expected] = GetParam();

  const auto result = jet / scalar;

  EXPECT_DOUBLE_EQ(primal(result), primal(expected));
  EXPECT_DOUBLE_EQ(derivative(result), derivative(expected));
}

TEST_P(JetTestElementDivision, JetDividedByElementInPlace) {
  auto [jet, scalar, expected] = GetParam();

  jet /= scalar;

  EXPECT_DOUBLE_EQ(primal(jet), primal(expected));
  EXPECT_DOUBLE_EQ(derivative(jet), derivative(expected));
}

const JetElementDivisionVector element_division_vectors[] = {
    {{10.0, 4.0}, 2.0, {5.0, 2.0}},
    {{3.0, 2.0}, 1.0, {3.0, 2.0}},
    {{3.0, 2.0}, -1.0, {-3.0, -2.0}},
    {{0.0, 0.0}, 5.0, {0.0, 0.0}},
};
INSTANTIATE_TEST_SUITE_P(ElementDivision, JetTestElementDivision,
                         ValuesIn(element_division_vectors));

struct JetTestElementDivisionByJet : JetTest {};

TEST_F(JetTestElementDivisionByJet, ElementDividedByJet) {
  // d(c/x) = -c/x^2 * dx
  // 6/3 = 2, derivative = -6/9 * 2 = -4/3
  const auto result = 6.0 / J{3.0, 2.0};

  EXPECT_DOUBLE_EQ(primal(result), 2.0);
  EXPECT_NEAR(derivative(result), -4.0 / 3.0, kEps);
}

// ============================================================================
// Jet Arithmetic
// ============================================================================

struct JetBinaryOpVector {
  using J = JetTest::J;

  J lhs;
  J rhs;
  J expected;

  friend auto operator<<(std::ostream& os, const JetBinaryOpVector& c)
      -> std::ostream& {
    return os << c.lhs << " ⨂ " << c.rhs << " = " << c.expected;
  }
};

// ----------------------------------------------------------------------------
// Addition
// ----------------------------------------------------------------------------

struct JetTestAddition : JetTest, WithParamInterface<JetBinaryOpVector> {};

TEST_P(JetTestAddition, Addition) {
  const auto [lhs, rhs, expected] = GetParam();

  const auto result = lhs + rhs;

  EXPECT_DOUBLE_EQ(primal(result), primal(expected));
  EXPECT_DOUBLE_EQ(derivative(result), derivative(expected));
}

TEST_P(JetTestAddition, InPlace) {
  auto [lhs, rhs, expected] = GetParam();

  lhs += rhs;

  EXPECT_DOUBLE_EQ(primal(lhs), primal(expected));
  EXPECT_DOUBLE_EQ(derivative(lhs), derivative(expected));
}

const JetBinaryOpVector jet_addition_vectors[] = {
    {{1.0, 2.0}, {3.0, 4.0}, {4.0, 6.0}},
    {{-1.0, 2.0}, {3.0, -4.0}, {2.0, -2.0}},
    {{0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}},
    {{5.0, 0.0}, {0.0, 3.0}, {5.0, 3.0}},
};
INSTANTIATE_TEST_SUITE_P(Addition, JetTestAddition,
                         ValuesIn(jet_addition_vectors));

// ----------------------------------------------------------------------------
// Subtraction
// ----------------------------------------------------------------------------

struct JetTestSubtraction : JetTest, WithParamInterface<JetBinaryOpVector> {};

TEST_P(JetTestSubtraction, Subtraction) {
  const auto [lhs, rhs, expected] = GetParam();

  const auto result = lhs - rhs;

  EXPECT_DOUBLE_EQ(primal(result), primal(expected));
  EXPECT_DOUBLE_EQ(derivative(result), derivative(expected));
}

TEST_P(JetTestSubtraction, InPlace) {
  auto [lhs, rhs, expected] = GetParam();

  lhs -= rhs;

  EXPECT_DOUBLE_EQ(primal(lhs), primal(expected));
  EXPECT_DOUBLE_EQ(derivative(lhs), derivative(expected));
}

const JetBinaryOpVector jet_subtraction_vectors[] = {
    {{5.0, 7.0}, {3.0, 4.0}, {2.0, 3.0}},
    {{1.0, 2.0}, {3.0, 4.0}, {-2.0, -2.0}},
    {{0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}},
};
INSTANTIATE_TEST_SUITE_P(Subtraction, JetTestSubtraction,
                         ValuesIn(jet_subtraction_vectors));

// ----------------------------------------------------------------------------
// Multiplication
// ----------------------------------------------------------------------------

struct JetTestMultiplication : JetTest,
                               WithParamInterface<JetBinaryOpVector> {};

TEST_P(JetTestMultiplication, Multiplication) {
  const auto [lhs, rhs, expected] = GetParam();

  const auto result = lhs * rhs;

  EXPECT_DOUBLE_EQ(primal(result), primal(expected));
  EXPECT_DOUBLE_EQ(derivative(result), derivative(expected));
}

TEST_P(JetTestMultiplication, InPlace) {
  auto [lhs, rhs, expected] = GetParam();

  lhs *= rhs;

  EXPECT_DOUBLE_EQ(primal(lhs), primal(expected));
  EXPECT_DOUBLE_EQ(derivative(lhs), derivative(expected));
}

// Product rule: d(u*v) = u*dv + du*v
// {a1, v1} * {a2, v2} = {a1*a2, a1*v2 + v1*a2}
const JetBinaryOpVector jet_multiplication_vectors[] = {
    // {3,2} * {5,7} = {15, 3*7 + 2*5} = {15, 31}
    {{3.0, 2.0}, {5.0, 7.0}, {15.0, 31.0}},

    // {2,1} * {3,1} = {6, 2*1 + 1*3} = {6, 5}
    {{2.0, 1.0}, {3.0, 1.0}, {6.0, 5.0}},

    // Zero cases
    {{0.0, 1.0}, {5.0, 7.0}, {0.0, 5.0}},
    {{3.0, 2.0}, {0.0, 0.0}, {0.0, 0.0}},

    // Identity
    {{3.0, 2.0}, {1.0, 0.0}, {3.0, 2.0}},
};
INSTANTIATE_TEST_SUITE_P(Multiplication, JetTestMultiplication,
                         ValuesIn(jet_multiplication_vectors));

// ----------------------------------------------------------------------------
// Division
// ----------------------------------------------------------------------------

struct JetTestDivision : JetTest, WithParamInterface<JetBinaryOpVector> {};

TEST_P(JetTestDivision, Division) {
  const auto [lhs, rhs, expected] = GetParam();

  const auto result = lhs / rhs;

  EXPECT_NEAR(primal(result), primal(expected), kEps);
  EXPECT_NEAR(derivative(result), derivative(expected), kEps);
}

TEST_P(JetTestDivision, InPlace) {
  auto [lhs, rhs, expected] = GetParam();

  lhs /= rhs;

  EXPECT_NEAR(primal(lhs), primal(expected), kEps);
  EXPECT_NEAR(derivative(lhs), derivative(expected), kEps);
}

// Quotient rule: d(u/v) = (du*v - u*dv)/v^2 = (du - (u/v)*dv)/v
// {a1, v1} / {a2, v2} = {a1/a2, (v1 - (a1/a2)*v2)/a2}
const JetBinaryOpVector jet_division_vectors[] = {
    // {6,5} / {2,1} = {3, (5 - 3*1)/2} = {3, 1}
    {{6.0, 5.0}, {2.0, 1.0}, {3.0, 1.0}},

    // {10,4} / {2,0} = {5, (4 - 5*0)/2} = {5, 2}
    {{10.0, 4.0}, {2.0, 0.0}, {5.0, 2.0}},

    // {3,2} / {1,0} = {3, 2} (identity divisor)
    {{3.0, 2.0}, {1.0, 0.0}, {3.0, 2.0}},

    // {0,0} / {5,7} = {0, 0}
    {{0.0, 0.0}, {5.0, 7.0}, {0.0, 0.0}},
};
INSTANTIATE_TEST_SUITE_P(Division, JetTestDivision,
                         ValuesIn(jet_division_vectors));

// ============================================================================
// Mixed Element Expressions
// ============================================================================

struct JetTestMixedExpressions : JetTest {};

TEST_F(JetTestMixedExpressions, PolynomialWithScalarCoefficients) {
  // f(x) = 2x^2 + 3x + 1
  // f'(x) = 4x + 3
  // At x = 2: f(2) = 8 + 6 + 1 = 15, f'(2) = 11
  const auto x = J{2.0, 1.0};

  const auto result = 2.0 * x * x + 3.0 * x + 1.0;

  EXPECT_DOUBLE_EQ(primal(result), 15.0);
  EXPECT_DOUBLE_EQ(derivative(result), 11.0);
}

TEST_F(JetTestMixedExpressions, RationalWithScalars) {
  // f(x) = (2x + 1) / (x + 3)
  // f'(x) = (2(x+3) - (2x+1)) / (x+3)^2 = 5 / (x+3)^2
  // At x = 2: f(2) = 5/5 = 1, f'(2) = 5/25 = 0.2
  const auto x = J{2.0, 1.0};

  const auto result = (2.0 * x + 1.0) / (x + 3.0);

  EXPECT_DOUBLE_EQ(primal(result), 1.0);
  EXPECT_DOUBLE_EQ(derivative(result), 0.2);
}

TEST_F(JetTestMixedExpressions, ExpWithScalarAddition) {
  // f(x) = exp(x + 1)
  // f'(x) = exp(x + 1)
  // At x = 0: f(0) = e, f'(0) = e
  const auto x = J{0.0, 1.0};

  const auto result = exp(x + 1.0);

  EXPECT_NEAR(primal(result), M_E, kEps);
  EXPECT_NEAR(derivative(result), M_E, kEps);
}

// ============================================================================
// Comparison
// ============================================================================

struct JetTestComparison : JetTest {};

TEST_F(JetTestComparison, EqualityWithElement) {
  // Equal only if primal matches AND derivative is zero
  EXPECT_TRUE((J{5.0, 0.0} == 5.0));
  EXPECT_FALSE((J{5.0, 1.0} == 5.0));  // derivative != 0
  EXPECT_FALSE((J{5.1, 0.0} == 5.0));  // primal != 5
}

TEST_F(JetTestComparison, ComparisonWithElement) {
  EXPECT_TRUE((J{3.0, 999.0} < 4.0));  // derivative ignored
  EXPECT_TRUE((J{5.0, 999.0} > 4.0));  // derivative ignored
  EXPECT_TRUE((J{4.0, 999.0} <= 4.0));
  EXPECT_TRUE((J{4.0, 999.0} >= 4.0));
}

TEST_F(JetTestComparison, EqualityBetweenJets) {
  EXPECT_TRUE((J{3.0, 2.0} == J{3.0, 2.0}));
  EXPECT_FALSE((J{3.0, 2.0} == J{3.0, 3.0}));  // different derivative
  EXPECT_FALSE((J{3.0, 2.0} == J{4.0, 2.0}));  // different primal
}

TEST_F(JetTestComparison, OrderingBetweenJets) {
  // Ordering ignores derivative (weak ordering)
  EXPECT_TRUE((J{3.0, 999.0} < J{4.0, 0.0}));
  EXPECT_TRUE((J{5.0, 0.0} > J{4.0, 999.0}));
  EXPECT_TRUE((J{4.0, 1.0} <= J{4.0, 2.0}));  // equal primals
  EXPECT_TRUE((J{4.0, 2.0} >= J{4.0, 1.0}));
}

// ============================================================================
// Selection
// ============================================================================

// ----------------------------------------------------------------------------
// Min, Max
// ----------------------------------------------------------------------------

struct SelectionMinMaxVector {
  using J = JetTest::J;

  J x;
  J y;
  J expected_min;
  J expected_max;

  friend auto operator<<(std::ostream& os, const SelectionMinMaxVector& c)
      -> std::ostream& {
    return os << "min/max(" << c.x << ", " << c.y << ")";
  }
};

struct JetTestSelectionMinMax : JetTest,
                                WithParamInterface<SelectionMinMaxVector> {};

TEST_P(JetTestSelectionMinMax, Min) {
  const auto [x, y, expected_min, expected_max] = GetParam();

  const auto result = min(x, y);

  EXPECT_DOUBLE_EQ(primal(result), primal(expected_min));
  EXPECT_DOUBLE_EQ(derivative(result), derivative(expected_min));
}

TEST_P(JetTestSelectionMinMax, Max) {
  const auto [x, y, expected_min, expected_max] = GetParam();

  const auto result = max(x, y);

  EXPECT_DOUBLE_EQ(primal(result), primal(expected_max));
  EXPECT_DOUBLE_EQ(derivative(result), derivative(expected_max));
}

const SelectionMinMaxVector selection_vectors[] = {
    // x < y: min=x, max=y
    {{2.0, 10.0}, {5.0, 20.0}, {2.0, 10.0}, {5.0, 20.0}},

    // x > y: min=y, max=x
    {{5.0, 10.0}, {2.0, 20.0}, {2.0, 20.0}, {5.0, 10.0}},

    // x == y: both return y (due to < condition)
    {{3.0, 10.0}, {3.0, 20.0}, {3.0, 20.0}, {3.0, 20.0}},
};
INSTANTIATE_TEST_SUITE_P(Selection, JetTestSelectionMinMax,
                         ValuesIn(selection_vectors));

// ----------------------------------------------------------------------------
// Clamp
// ----------------------------------------------------------------------------

struct JetTestSelectionClamp : JetTest {};

TEST_F(JetTestSelectionClamp, Below) {
  const auto result = clamp(J{1.0, 3.0}, J{2.0, 10.0}, J{8.0, 20.0});

  EXPECT_DOUBLE_EQ(primal(result), 2.0);
  EXPECT_DOUBLE_EQ(derivative(result), 10.0);  // returns lo
}

TEST_F(JetTestSelectionClamp, Above) {
  const auto result = clamp(J{10.0, 3.0}, J{2.0, 10.0}, J{8.0, 20.0});

  EXPECT_DOUBLE_EQ(primal(result), 8.0);
  EXPECT_DOUBLE_EQ(derivative(result), 20.0);  // returns hi
}

TEST_F(JetTestSelectionClamp, Within) {
  const auto result = clamp(J{5.0, 3.0}, J{2.0, 10.0}, J{8.0, 20.0});

  EXPECT_DOUBLE_EQ(primal(result), 5.0);
  EXPECT_DOUBLE_EQ(derivative(result), 3.0);  // returns x
}

// ============================================================================
// Classification
// ============================================================================

struct JetTestClassification : JetTest {};

TEST_F(JetTestClassification, IsFinite) {
  EXPECT_TRUE(isfinite(J{1.0, 2.0}));
  EXPECT_TRUE(isfinite(J{0.0, 0.0}));
  EXPECT_FALSE(isfinite(J{inf_e, 0.0}));
  EXPECT_FALSE(isfinite(J{0.0, inf_e}));
  EXPECT_FALSE(isfinite(J{nan_e, 0.0}));
  EXPECT_FALSE(isfinite(J{0.0, nan_e}));
}

TEST_F(JetTestClassification, IsNan) {
  EXPECT_FALSE(isnan(J{1.0, 2.0}));
  EXPECT_FALSE(isnan(J{inf_e, 0.0}));
  EXPECT_TRUE(isnan(J{nan_e, 0.0}));
  EXPECT_TRUE(isnan(J{0.0, nan_e}));
  EXPECT_TRUE(isnan(J{nan_e, nan_e}));
}

// ============================================================================
// Math Functions
// ============================================================================

struct MathFuncVector {
  using E = JetTest::E;

  E input;
  E expected_primal;
  E expected_derivative;

  friend auto operator<<(std::ostream& os, const MathFuncVector& c)
      -> std::ostream& {
    return os << "f(" << c.input << ") = {" << c.expected_primal << ", "
              << c.expected_derivative << "}";
  }
};

// ----------------------------------------------------------------------------
// abs
// ----------------------------------------------------------------------------

// d(|x|) = sgn(x)
struct JetTestAbs : JetTest, WithParamInterface<MathFuncVector> {};

TEST_P(JetTestAbs, Derivative) {
  const auto [a, f_a, df_a] = GetParam();

  const auto result = abs(seed(a));

  EXPECT_DOUBLE_EQ(primal(result), f_a);
  EXPECT_DOUBLE_EQ(derivative(result), df_a);
}

const MathFuncVector abs_vectors[] = {
    {5.0, 5.0, 1.0},
    {-5.0, 5.0, -1.0},
    {0.0, 0.0, 1.0},  // copysign(1, +0) = 1
};
INSTANTIATE_TEST_SUITE_P(Abs, JetTestAbs, ValuesIn(abs_vectors));

// ----------------------------------------------------------------------------
// cos
// ----------------------------------------------------------------------------

// d(cos(x)) = -sin(x)*dx
struct JetTestCos : JetTest, WithParamInterface<MathFuncVector> {};

TEST_P(JetTestCos, Derivative) {
  const auto [a, f_a, df_a] = GetParam();

  const auto result = cos(seed(a));

  EXPECT_NEAR(primal(result), f_a, kEps);
  EXPECT_NEAR(derivative(result), df_a, kEps);
}

const MathFuncVector cos_vectors[] = {
    {0.0, 1.0, 0.0},
    {M_PI / 3, cos(M_PI / 3), -sin(M_PI / 3)},
    {M_PI / 5, cos(M_PI / 5), -sin(M_PI / 5)},
    {M_PI_2, 0.0, -1.0},
    {M_PI, -1.0, 0.0},
    {-M_PI_2, 0.0, 1.0},
};
INSTANTIATE_TEST_SUITE_P(Cos, JetTestCos, ValuesIn(cos_vectors));

// ----------------------------------------------------------------------------
// exp
// ----------------------------------------------------------------------------

// d(exp(x)) = exp(x)
struct JetTestExp : JetTest, WithParamInterface<MathFuncVector> {};

TEST_P(JetTestExp, Derivative) {
  const auto [a, f_a, df_a] = GetParam();

  const auto result = exp(seed(a));

  EXPECT_NEAR(primal(result), f_a, kEps);
  EXPECT_NEAR(derivative(result), df_a, kEps);
}

const MathFuncVector exp_vectors[] = {
    {0.0, 1.0, 1.0},
    {1.0, M_E, M_E},
    {-1.0, 1.0 / M_E, 1.0 / M_E},
    {2.0, M_E* M_E, M_E* M_E},
};
INSTANTIATE_TEST_SUITE_P(Exp, JetTestExp, ValuesIn(exp_vectors));

// ----------------------------------------------------------------------------
// log
// ----------------------------------------------------------------------------

// d(log(x)) = 1/x
struct JetTestLog : JetTest, WithParamInterface<MathFuncVector> {};

TEST_P(JetTestLog, Derivative) {
  const auto [a, f_a, df_a] = GetParam();

  const auto result = log(seed(a));

  EXPECT_NEAR(primal(result), f_a, kEps);
  EXPECT_NEAR(derivative(result), df_a, kEps);
}

const MathFuncVector log_vectors[] = {
    {1.0, 0.0, 1.0},
    {M_E, 1.0, 1.0 / M_E},
    {2.0, std::log(2.0), 0.5},
    {10.0, std::log(10.0), 0.1},
};
INSTANTIATE_TEST_SUITE_P(Log, JetTestLog, ValuesIn(log_vectors));

// ----------------------------------------------------------------------------
// log1p
// ----------------------------------------------------------------------------

// d(log1p(x)) = 1/(x+1)
struct JetTestLog1p : JetTest, WithParamInterface<MathFuncVector> {};

TEST_P(JetTestLog1p, Derivative) {
  const auto [a, f_a, df_a] = GetParam();

  const auto result = log1p(seed(a));

  EXPECT_NEAR(primal(result), f_a, kEps);
  EXPECT_NEAR(derivative(result), df_a, kEps);
}

const MathFuncVector log1p_vectors[] = {
    // log1p(0) = 0, 1/(0+1) = 1
    {0.0, 0.0, 1.0},

    // log1p(1) = ln(2), 1/2
    {1.0, std::log(2.0), 0.5},

    // log1p(e-1) = 1
    {M_E - 1.0, 1.0, 1.0 / M_E},
};
INSTANTIATE_TEST_SUITE_P(Log1p, JetTestLog1p, ValuesIn(log1p_vectors));

// ----------------------------------------------------------------------------
// sin
// ----------------------------------------------------------------------------

// d(sin(x)) = cos(x)*dx
struct JetTestSin : JetTest, WithParamInterface<MathFuncVector> {};

TEST_P(JetTestSin, Derivative) {
  const auto [a, f_a, df_a] = GetParam();

  const auto result = sin(seed(a));

  EXPECT_NEAR(primal(result), f_a, kEps);
  EXPECT_NEAR(derivative(result), df_a, kEps);
}

const MathFuncVector sin_vectors[] = {
    {0.0, 0.0, 1.0},
    {M_PI / 3, sin(M_PI / 3), cos(M_PI / 3)},
    {M_PI / 5, sin(M_PI / 5), cos(M_PI / 5)},
    {M_PI_2, 1.0, 0.0},
    {M_PI, 0.0, -1.0},
    {-M_PI_2, -1.0, 0.0},
};
INSTANTIATE_TEST_SUITE_P(Sin, JetTestSin, ValuesIn(sin_vectors));

// ----------------------------------------------------------------------------
// sqrt
// ----------------------------------------------------------------------------

struct JetTestSqrt : JetTest {};

TEST_F(JetTestSqrt, ZeroDerivativeIsInfinity) {
  const auto result = sqrt(seed(0.0));

  EXPECT_DOUBLE_EQ(primal(result), 0.0);
  EXPECT_TRUE(std::isinf(derivative(result)));
}

// d(sqrt(x)) = 1/(2*sqrt(x))
struct JetTestSqrtParameterized : JetTest,
                                  WithParamInterface<MathFuncVector> {};

TEST_P(JetTestSqrtParameterized, Derivative) {
  const auto [a, f_a, df_a] = GetParam();

  const auto result = sqrt(seed(a));

  EXPECT_NEAR(primal(result), f_a, kEps);
  EXPECT_NEAR(derivative(result), df_a, kEps);
}

const MathFuncVector sqrt_vectors[] = {
    {1.0, 1.0, 0.5},
    {4.0, 2.0, 0.25},
    {9.0, 3.0, 1.0 / 6.0},
    {0.25, 0.5, 1.0},
};
INSTANTIATE_TEST_SUITE_P(Sqrt, JetTestSqrtParameterized,
                         ValuesIn(sqrt_vectors));

// ----------------------------------------------------------------------------
// tan
// ----------------------------------------------------------------------------

// d(tan(x)) = (1 + tan(x)^2)*dx
struct JetTestTan : JetTest, WithParamInterface<MathFuncVector> {};

TEST_P(JetTestTan, Derivative) {
  const auto [a, f_a, df_a] = GetParam();

  const auto result = tan(seed(a));

  EXPECT_NEAR(primal(result), f_a, kEps);
  EXPECT_NEAR(derivative(result), df_a, kEps);
}

const MathFuncVector tan_vectors[] = {
    {0.0, 0.0, 1.0},
    {M_PI / 3, tan(M_PI / 3), (1 + tan(M_PI / 3) * tan(M_PI / 3))},
    {M_PI / 5, tan(M_PI / 5), (1 + tan(M_PI / 5) * tan(M_PI / 5))},
    {M_PI_4, 1.0, 2.0},
    {-M_PI_4, -1.0, 2.0},
};
INSTANTIATE_TEST_SUITE_P(Tan, JetTestTan, ValuesIn(tan_vectors));

// ----------------------------------------------------------------------------
// tanh
// ----------------------------------------------------------------------------

// d(tanh(x)) = 1 - tanh(x)^2
struct JetTestTanh : JetTest, WithParamInterface<MathFuncVector> {};

TEST_P(JetTestTanh, Derivative) {
  const auto [a, f_a, df_a] = GetParam();

  const auto result = tanh(seed(a));

  EXPECT_NEAR(primal(result), f_a, kEps);
  EXPECT_NEAR(derivative(result), df_a, kEps);
}

const MathFuncVector tanh_vectors[] = {
    // tanh(0) = 0, sech^2(0) = 1
    {0.0, 0.0, 1.0},

    {1.0, std::tanh(1.0), 1.0 - std::tanh(1.0) * std::tanh(1.0)},
    {-1.0, std::tanh(-1.0), 1.0 - std::tanh(-1.0) * std::tanh(-1.0)},
};
INSTANTIATE_TEST_SUITE_P(Tanh, JetTestTanh, ValuesIn(tanh_vectors));

// ----------------------------------------------------------------------------
// hypot
// ----------------------------------------------------------------------------

// d(hypot(x,y)) = (x*dx + y*dy) / hypot(x,y)
struct JetTestHypot : JetTest {};

TEST_F(JetTestHypot, SeedX) {
  // hypot(3, 4) = 5
  // d/dx hypot(x, 4)|_{x=3} = 3/5 = 0.6
  const auto jx = J{3.0, 1.0};  // seed x
  const auto jy = J{4.0, 0.0};  // constant y

  const auto result = hypot(jx, jy);

  EXPECT_DOUBLE_EQ(primal(result), 5.0);
  EXPECT_DOUBLE_EQ(derivative(result), 0.6);
}

TEST_F(JetTestHypot, SeedY) {
  // d/dy hypot(3, y)|_{y=4} = 4/5 = 0.8
  const auto jx = J{3.0, 0.0};
  const auto jy = J{4.0, 1.0};  // seed y

  const auto result = hypot(jx, jy);

  EXPECT_DOUBLE_EQ(primal(result), 5.0);
  EXPECT_DOUBLE_EQ(derivative(result), 0.8);
}

TEST_F(JetTestHypot, BothSeeded) {
  // If both are seeded (same variable), d = (x + y) / hypot(x,y)
  const auto jx = J{3.0, 1.0};
  const auto jy = J{4.0, 1.0};

  const auto result = hypot(jx, jy);

  EXPECT_DOUBLE_EQ(primal(result), 5.0);
  EXPECT_DOUBLE_EQ(derivative(result), (3.0 + 4.0) / 5.0);
}

TEST_F(JetTestHypot, Zero) {
  const auto result = hypot(J{0.0, 1.0}, J{0.0, 1.0});

  EXPECT_DOUBLE_EQ(primal(result), 0.0);
  EXPECT_DOUBLE_EQ(derivative(result), 0.0);  // special case
}

// ----------------------------------------------------------------------------
// pow
// ----------------------------------------------------------------------------

struct JetTestPow : JetTest {};

// pow(Jet, Element)
// d(x^n) = n * x^(n-1)
struct JetTestPowJetElement : JetTestPow {};

TEST_F(JetTestPowJetElement, Square) {
  // d/dx x^2|_{x=5} = 2 * 5 = 10
  const auto result = pow(seed(5.0), 2.0);

  EXPECT_DOUBLE_EQ(primal(result), 25.0);
  EXPECT_DOUBLE_EQ(derivative(result), 10.0);
}

TEST_F(JetTestPowJetElement, Cube) {
  // d/dx x^3|_{x=2} = 3 * 2^2 = 12
  const auto result = pow(seed(2.0), 3.0);

  EXPECT_DOUBLE_EQ(primal(result), 8.0);
  EXPECT_DOUBLE_EQ(derivative(result), 12.0);
}

TEST_F(JetTestPowJetElement, Sqrt) {
  // d/dx x^0.5|_{x=4} = 0.5 * 4^(-0.5) = 0.5 * 0.5 = 0.25
  const auto result = pow(seed(4.0), 0.5);

  EXPECT_DOUBLE_EQ(primal(result), 2.0);
  EXPECT_DOUBLE_EQ(derivative(result), 0.25);
}

// pow(Element, Jet)
// d(b^y) = ln(b) * b^y
struct JetTestPowElementJet : JetTestPow {};

TEST_F(JetTestPowElementJet, Cube) {
  // d/dy 2^y|_{y=3} = ln(2) * 2^3 = ln(2) * 8
  const auto result = pow(2.0, seed(3.0));

  EXPECT_NEAR(primal(result), 8.0, kEps);
  EXPECT_NEAR(derivative(result), std::log(2.0) * 8.0, kEps);
}

TEST_F(JetTestPowElementJet, BaseE) {
  // d/dy e^y|_{y=2} = ln(e) * e^2 = e^2
  const auto result = pow(M_E, seed(2.0));

  EXPECT_NEAR(primal(result), M_E * M_E, kEps);
  EXPECT_NEAR(derivative(result), M_E * M_E, kEps);
}

// pow(Jet, Jet)
// d(x^y) = x^y * (ln(x)*dy + y*dx/x)
struct JetTestPowJetJet : JetTestPow {};

TEST_F(JetTestPowJetJet, SeedNeither) {
  // d/dx x^3|_{x=2} with x constant and y constant = 3 * 2 = 8
  const auto base = J{2.0, 0.0};  // constant
  const auto exp = J{3.0, 0.0};   // constant

  const auto result = pow(base, exp);

  EXPECT_NEAR(primal(result), 8.0, kEps);
  EXPECT_NEAR(derivative(result), 0.0, kEps);
}

TEST_F(JetTestPowJetJet, SeedBase) {
  // d/dx x^3|_{x=2} with y constant = 3 * 2^2 = 12
  const auto base = J{2.0, 1.0};  // seed
  const auto exp = J{3.0, 0.0};   // constant

  const auto result = pow(base, exp);

  EXPECT_NEAR(primal(result), 8.0, kEps);
  EXPECT_NEAR(derivative(result), 12.0, kEps);
}

TEST_F(JetTestPowJetJet, SeedExponent) {
  // d/dy 2^y|_{y=3} with x=2 constant = ln(2) * 8
  const auto base = J{2.0, 0.0};  // constant
  const auto exp = J{3.0, 1.0};   // seed

  const auto result = pow(base, exp);

  EXPECT_NEAR(primal(result), 8.0, kEps);
  EXPECT_NEAR(derivative(result), std::log(2.0) * 8.0, kEps);
}

TEST_F(JetTestPowJetJet, SeedBoth) {
  // f(t) = t^t at t=2
  // f(2) = 4
  // f'(t) = t^t * (ln(t) + 1)
  // f'(2) = 4 * (ln(2) + 1)
  const auto t = J{2.0, 1.0};

  const auto result = pow(t, t);

  EXPECT_NEAR(primal(result), 4.0, kEps);
  EXPECT_NEAR(derivative(result), 4.0 * (std::log(2.0) + 1.0), kEps);
}

// ----------------------------------------------------------------------------
// copysign
// ----------------------------------------------------------------------------

struct JetTestCopysign : JetTest {};

TEST_F(JetTestCopysign, PositivePositive) {
  const auto result = copysign(J{3.0, 2.0}, J{5.0, 0.0});

  EXPECT_DOUBLE_EQ(primal(result), 3.0);
  EXPECT_DOUBLE_EQ(derivative(result), 2.0);  // sgn(+)*sgn(+) = +1
}

TEST_F(JetTestCopysign, PositiveNegative) {
  const auto result = copysign(J{3.0, 2.0}, J{-5.0, 0.0});

  EXPECT_DOUBLE_EQ(primal(result), -3.0);
  EXPECT_DOUBLE_EQ(derivative(result), -2.0);  // sgn(+)*sgn(-) = -1
}

TEST_F(JetTestCopysign, NegativePositive) {
  const auto result = copysign(J{-3.0, 2.0}, J{5.0, 0.0});

  EXPECT_DOUBLE_EQ(primal(result), 3.0);
  EXPECT_DOUBLE_EQ(derivative(result), -2.0);  // sgn(-)*sgn(+) = -1
}

TEST_F(JetTestCopysign, NegativeNegative) {
  const auto result = copysign(J{-3.0, 2.0}, J{-5.0, 0.0});

  EXPECT_DOUBLE_EQ(primal(result), -3.0);
  EXPECT_DOUBLE_EQ(derivative(result), 2.0);  // sgn(-)*sgn(-) = +1
}

TEST_F(JetTestCopysign, ZeroSign) {
  // When sgn.a == 0, the derivative includes a delta spike to infinity.
  const auto result = copysign(J{3.0, 1.0}, J{0.0, 1.0});

  EXPECT_DOUBLE_EQ(primal(result), 3.0);
  EXPECT_TRUE(std::isinf(derivative(result)));
}

// ============================================================================
// numeric_limits
// ============================================================================

struct JetTestNumericLimits : JetTest {};

TEST_F(JetTestNumericLimits, Specialized) {
  EXPECT_TRUE(std::numeric_limits<J>::is_specialized);
}

TEST_F(JetTestNumericLimits, Min) {
  const auto min_j = std::numeric_limits<J>::min();

  EXPECT_EQ(primal(min_j), std::numeric_limits<E>::min());
}

TEST_F(JetTestNumericLimits, Max) {
  const auto max_j = std::numeric_limits<J>::max();

  EXPECT_EQ(primal(max_j), std::numeric_limits<E>::max());
}

TEST_F(JetTestNumericLimits, Infinity) {
  const auto inf_j = std::numeric_limits<J>::infinity();

  EXPECT_TRUE(std::isinf(primal(inf_j)));
}

TEST_F(JetTestNumericLimits, QuietNaN) {
  const auto nan_j = std::numeric_limits<J>::quiet_NaN();

  EXPECT_TRUE(std::isnan(primal(nan_j)));
}

TEST_F(JetTestNumericLimits, Epsilon) {
  const auto eps_j = std::numeric_limits<J>::epsilon();

  EXPECT_EQ(primal(eps_j), std::numeric_limits<E>::epsilon());
}

// ============================================================================
// Standard Library Integration
// ============================================================================

// ----------------------------------------------------------------------------
// Stream Output
// ----------------------------------------------------------------------------

struct JetTestStreamOutput : JetTest {};

TEST_F(JetTestStreamOutput, Format) {
  std::ostringstream out;

  out << J{3.5, 2.5};

  EXPECT_EQ(out.str(), "{.a = 3.5, .v = 2.5}");
}

// ----------------------------------------------------------------------------
// Swap
// ----------------------------------------------------------------------------

struct JetTestSwap : JetTest {};

TEST_F(JetTestSwap, Swap) {
  auto a = J{1.0, 2.0};
  auto b = J{3.0, 4.0};

  swap(a, b);

  EXPECT_EQ(primal(a), 3.0);
  EXPECT_EQ(derivative(a), 4.0);
  EXPECT_EQ(primal(b), 1.0);
  EXPECT_EQ(derivative(b), 2.0);
}

// ============================================================================
// Chain Rule Composition
// ============================================================================

struct JetTestChainRuleComposition : JetTest {};

TEST_F(JetTestChainRuleComposition, ExpLog) {
  // exp(log(x)) = x, so d/dx = 1
  const auto result = exp(log(seed(5.0)));

  EXPECT_NEAR(primal(result), 5.0, kEps);
  EXPECT_NEAR(derivative(result), 1.0, kEps);
}

TEST_F(JetTestChainRuleComposition, SqrtSquare) {
  // sqrt(x^2) = |x| for x > 0, d/dx = 1
  const auto result = sqrt(pow(seed(3.0), 2.0));

  EXPECT_NEAR(primal(result), 3.0, kEps);
  EXPECT_NEAR(derivative(result), 1.0, kEps);
}

TEST_F(JetTestChainRuleComposition, TanhExp) {
  // d/dx tanh(exp(x))|_{x=0}
  // = sech^2(exp(0)) * exp(0)
  // = sech^2(1) * 1
  // = 1 - tanh(1)^2
  const auto result = tanh(exp(seed(0.0)));

  const auto tanh_1 = std::tanh(1.0);
  EXPECT_NEAR(primal(result), tanh_1, kEps);
  EXPECT_NEAR(derivative(result), 1.0 - tanh_1 * tanh_1, kEps);
}

TEST_F(JetTestChainRuleComposition, LogSqrt) {
  // log(sqrt(x)) = 0.5 * log(x)
  // d/dx = 0.5 / x
  const auto result = log(sqrt(seed(4.0)));

  EXPECT_NEAR(primal(result), 0.5 * std::log(4.0), kEps);
  EXPECT_NEAR(derivative(result), 0.5 / 4.0, kEps);
}

// ============================================================================
// Assertion Death Tests
// ============================================================================

#if !defined NDEBUG

struct JetDeathTest : JetTest {};

TEST_F(JetDeathTest, LogNegativeDomain) {
  EXPECT_DEBUG_DEATH(log(seed(-1.0)), "domain error");
}

TEST_F(JetDeathTest, Log1pBelowNegativeOne) {
  EXPECT_DEBUG_DEATH(log1p(seed(-2.0)), "domain error");
}

TEST_F(JetDeathTest, PowJetElementNegativeBase) {
  EXPECT_DEBUG_DEATH(pow(seed(-0.5), 2.5), "domain error");
}

TEST_F(JetDeathTest, PowJetElementNegativeIntegerBase) {
  EXPECT_DEBUG_DEATH(pow(seed(-1.0), 2.5), "domain error");
}

TEST_F(JetDeathTest, PowElementJetNegativeBase) {
  EXPECT_DEBUG_DEATH(pow(-2.0, seed(1.0)), "domain error");
}

TEST_F(JetDeathTest, PowJetJetNegativeBase) {
  EXPECT_DEBUG_DEATH(pow(seed(-1.0), J{2.0, 0.0}), "domain error");
}

TEST_F(JetDeathTest, SqrtNegative) {
  EXPECT_DEBUG_DEATH(sqrt(seed(-1.0)), "domain error");
}

#endif  // NDEBUG

}  // namespace
}  // namespace curves::math
