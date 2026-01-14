// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "transfer_function.hpp"
#include <curves/testing/test.hpp>
#include <curves/math/jet.hpp>
#include <curves/testing/linear_curve.hpp>

namespace curves::transfer_function {
namespace {

struct TransferFunctionTest : Test {
  using Scalar = double;
  using Jet = math::Jet<Scalar>;

  static constexpr auto jet_result = Jet{3, 5};
};

// ============================================================================
// TransferGradient Specialization
// ============================================================================

struct TransferFunctionTransferGradientTest : TransferFunctionTest {
  struct Integral {
    using Scalar = Scalar;

    auto operator()(Jet jet) const noexcept -> Jet { return jet; }
  };
  Integral integral;

  using Sut = TransferFunction<CurveDefinition::kTransferGradient, Integral>;
  const Sut sut{integral};
};

TEST_F(TransferFunctionTransferGradientTest, JetForwardedCorrectly) {
  ASSERT_EQ(jet_result, sut(jet_result));
}

// ============================================================================
// VelocityScale Specialization
// ============================================================================

struct TransferFunctionVelocityScaleTest : TransferFunctionTest {};

// ----------------------------------------------------------------------------
// Derivative Near 0
// ----------------------------------------------------------------------------

struct TransferFunctionVelocityScaleTestDerivativeNear0
    : TransferFunctionVelocityScaleTest {
  static constexpr auto scalar_result = Scalar{7};

  struct Curve {
    using Scalar = Scalar;

    auto operator()(Jet) const noexcept -> Jet { return jet_result; }
    auto operator()(Scalar) const noexcept -> Scalar { return scalar_result; }
  };
  Curve curve;

  using Sut = TransferFunction<CurveDefinition::kVelocityScale, Curve>;
  const Sut sut{curve};
};

TEST_F(TransferFunctionVelocityScaleTestDerivativeNear0,
       ScalarAwayFrom0UsesScalarEvaluation) {
  EXPECT_EQ(scalar_result, sut(1.0));
}

TEST_F(TransferFunctionVelocityScaleTestDerivativeNear0,
       ScalarAtFrom0UsesScalarEvaluation) {
  EXPECT_EQ(0.0, sut(0.0));
}

TEST_F(TransferFunctionVelocityScaleTestDerivativeNear0,
       JetAwayFrom0UsesJetEvaluation) {
  EXPECT_EQ((Jet{jet_result.a, jet_result.a + jet_result.v}),
            sut(Jet{1.0, 1.0}));
}

TEST_F(TransferFunctionVelocityScaleTestDerivativeNear0,
       JetAt0UsesLimitDefinition) {
  const auto derivative = Scalar{3};
  EXPECT_EQ((Jet{0.0, scalar_result * derivative}), sut(Jet{0.0, derivative}));
}

// ----------------------------------------------------------------------------
// Linear Curve
// ----------------------------------------------------------------------------

struct TransferFunctionVelocityScaleTestLinear
    : TransferFunctionVelocityScaleTest {
  // Arbitrary, nondegenerate curve, y = 3x + 5.
  using Curve = LinearCurve<Scalar>;
  using CriticalPoints = Curve::CriticalPoints;

  CriticalPoints critical_points{73, 79, 179, 181};
  Curve curve{3, 5, critical_points};

  using Sut = TransferFunction<CurveDefinition::kVelocityScale, Curve>;
  const Sut sut{curve};
};

TEST_F(TransferFunctionVelocityScaleTestLinear, ExerciseJetAt0) {
  const auto v = Jet{7, 11};

  /*
    v = {7, 11}
    S(v) = 3v + 5
    S'(v) = 3v
    T(v) = vS(v) = v(3v + 5)
      = 7(3*7 + 5) = 182
    T'(v) = v'S(v) + vS'(v) = v'(3v + 5) + v(3v') = 6vv' + 5v'
      = 6*7*11 + 5*11 = 517
  */
  const auto expected = Jet{182, 517};

  auto actual = sut(v);

  EXPECT_DOUBLE_EQ(expected.a, actual.a);
  EXPECT_DOUBLE_EQ(expected.v, actual.v);
}

TEST_F(TransferFunctionVelocityScaleTestLinear, PropagatesJet) {
  const auto v = Jet{7, 11};

  /*
    v = {7, 11}
    S(v) = 3v + 5
    S'(v) = 3v
    T(v) = vS(v) = v(3v + 5)
      = 7(3*7 + 5) = 182
    T'(v) = v'S(v) + vS'(v) = v'(3v + 5) + v(3v') = 6vv' + 5v'
      = 6*7*11 + 5*11 = 517
  */
  const auto expected = Jet{182, 517};

  auto actual = sut(v);

  EXPECT_DOUBLE_EQ(expected.a, actual.a);
  EXPECT_DOUBLE_EQ(expected.v, actual.v);
}

}  // namespace
}  // namespace curves::transfer_function
