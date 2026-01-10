// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "from_velocity_scale.hpp"
#include <curves/testing/test.hpp>
#include <curves/math/jet.hpp>
#include <curves/testing/linear_curve.hpp>

namespace curves::transfer_function {
namespace {

struct FromVelocityScaleTest : Test {
  using Scalar = double;
  using Jet = math::Jet<Scalar>;

  // Arbitrary, nondegenerate curve, y = 3x + 5.
  using Curve = LinearCurve<Scalar>;
  using CriticalPoints = Curve::CriticalPoints;

  CriticalPoints critical_points{73, 79, 179, 181};
  Curve curve{3, 5, critical_points};

  using Sut = FromVelocityScale<Scalar, Curve>;
  const Sut sut{curve};
};

TEST_F(FromVelocityScaleTest, PropagatesJet) {
  const auto x = Jet{7, 11};

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

  auto actual = sut(x);

  EXPECT_DOUBLE_EQ(expected.a, actual.a);
  EXPECT_DOUBLE_EQ(expected.v, actual.v);
}

TEST_F(FromVelocityScaleTest, ForwardsCriticalPoints) {
  const auto domain_max = 100;  // Includes only knots <= 100.
  const auto expected = CriticalPoints{73, 79};

  const auto actual = sut.critical_points(domain_max);

  ASSERT_EQ(expected, actual);
}

}  // namespace
}  // namespace curves::transfer_function
