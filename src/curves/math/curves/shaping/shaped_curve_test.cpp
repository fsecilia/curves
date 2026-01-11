// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2026 Frank Secilia
*/

#include "shaped_curve.hpp"
#include <curves/testing/test.hpp>
#include <curves/testing/linear_curve.hpp>
#include <gmock/gmock.h>

namespace curves::shaping {
namespace {

struct ShapedCurveTest : Test {
  using Scalar = double;
  using Value = double;
  using Curve = LinearCurve<Scalar>;
  using CriticalPoints = typename Curve::CriticalPoints;
  using Sut = ShapedCurve<Curve, Curve, Curve>;

  static auto make_identity(CriticalPoints critical_points = {}) noexcept
      -> Curve {
    return curves::make_identity<Scalar>(std::move(critical_points));
  }
};

TEST_F(ShapedCurveTest, IdentityCompositionPreservesCriticalPoints) {
  // Curve has critical points at 1.0 and 2.0.
  const auto curve = make_identity({1.0, 2.0});

  // Domain warp is 1:1.
  const auto sut = Sut{
      curve,
      make_identity(),  // EaseIn
      make_identity()   // EaseOut
  };

  // Domain max is high enough to include everything
  const auto critical_points = sut.critical_points(10.0);

  // Critical points should come out exactly as they went in.
  ASSERT_EQ(critical_points.size(), 2);
  EXPECT_DOUBLE_EQ(critical_points[0], 1.0);
  EXPECT_DOUBLE_EQ(critical_points[1], 2.0);
}

TEST_F(ShapedCurveTest, EaseInDelayShiftsPointsRight) {
  // Curve has a critical point at 5.0.
  const auto curve = make_identity({5.0});

  // EaseIn is a shift, acting as a delay.
  // It subtracts 2.0 from the input: y = x - 2, x = y + 2
  // So, to reach 5.0 on the curve, we need input 7.0.
  const auto ease_in = make_shift(-2.0);

  const auto sut = Sut{curve, ease_in, make_identity()};

  const auto critical_points = sut.critical_points(10.0);
  ASSERT_EQ(critical_points.size(), 1);
  EXPECT_DOUBLE_EQ(critical_points[0], 7.0);  // 5.0 - (-2.0)
}

TEST_F(ShapedCurveTest, EaseOutScalingSquashesPoints) {
  // Curve has a critical point at 4.0.
  const auto curve = make_identity({4.0});

  // EaseOut is a scale, acting as a multiplier.
  // It doubles the input: y = 2x, x = y/2
  // So, to reach 4.0 on the curve, we only need an input of 2.0.
  const auto ease_out = make_scale(2.0);

  const auto sut = Sut{curve, make_identity(), ease_out};

  const auto critical_points = sut.critical_points(10.0);
  ASSERT_EQ(critical_points.size(), 1);
  EXPECT_DOUBLE_EQ(critical_points[0], 2.0);  // 4.0/2.0
}

TEST_F(ShapedCurveTest, FullCompositionChain) {
  // Curve has critical point at 10.0.
  const auto curve = make_identity({10.0});

  // EaseIn is a delay of 3: y = x - 3, x = y + 3
  // Curve critical point transforms from 10.0 to 13.0 here.
  const auto ease_in = make_shift(-3.0);

  // EaseOut is a multiplier of 2: y = 2x, x = y/2
  // Curve critical point transforms from 13.0 to 6.5 here.
  const auto ease_out = make_scale(2.0);

  const auto sut = Sut{curve, ease_in, ease_out};

  const auto critical_points = sut.critical_points(100.0);
  ASSERT_EQ(critical_points.size(), 1);
  EXPECT_DOUBLE_EQ(critical_points[0], 6.5);
}

TEST_F(ShapedCurveTest, AggregatesPointsFromAllLayers) {
  // Curve has a critical point at 4.0.
  // This is in the domain after EaseIn and EaseOut.
  // It maps to 10.5 then 3.5.
  const auto curve = make_identity({5.5});

  // EaseIn critical point at 2.0.
  // This is in the domain after EaseOut.
  // It maps to 1.0.
  // y = x - 5, x = y + 5
  const auto ease_in = make_shift(-5.0, {3.0});

  // EaseOut critical point at 0.5.
  // This is in the final domain already.
  // It maps to 0.5.
  // y = 3x, x = y/3
  const auto ease_out = make_scale(3.0, {0.5});

  const auto sut = Sut{curve, ease_in, ease_out};

  const auto critical_points = sut.critical_points(10.0);

  ASSERT_EQ(critical_points.size(), 3);
  EXPECT_DOUBLE_EQ(critical_points[0], 0.5);
  EXPECT_DOUBLE_EQ(critical_points[1], 1.0);
  EXPECT_DOUBLE_EQ(critical_points[2], 3.5);
}

TEST_F(ShapedCurveTest, FiltersPointsOutsideDomain) {
  // 15.0 will be out of bounds.
  const auto curve = make_identity({5.0, 10.0, 11.0, 15.0});

  const auto sut = Sut{curve, make_identity(), make_identity()};

  // Limit domain to 10.0.
  const auto critical_points = sut.critical_points(10.0);

  // 15.0 should be gone.
  ASSERT_EQ(critical_points.size(), 2);
  EXPECT_DOUBLE_EQ(critical_points[0], 5.0);
  EXPECT_DOUBLE_EQ(critical_points[1], 10.0);
}

TEST_F(ShapedCurveTest, DeduplicatesClosePoints) {
  // Two points very close together.
  const auto curve = make_identity({1.0, 1.00000000000001});

  const auto sut = Sut{curve, make_identity(), make_identity()};

  const auto critical_points = sut.critical_points(10.0);
  ASSERT_EQ(critical_points.size(), 1);
  EXPECT_DOUBLE_EQ(critical_points[0], 1.0);
}

}  // namespace
}  // namespace curves::shaping
