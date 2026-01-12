// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "antiderivative.hpp"
#include <curves/testing/test.hpp>
#include <gmock/gmock.h>

namespace curves {
namespace {

using Scalar = double;
using Jet = math::Jet<Scalar>;

/*
  The test doesn't actually integrate. We just test this for identity to make
  sure it is being forwarded correctly.
*/
struct Integrator {
  static const auto expected_identity = int_t{3};
  int_t identity = 0;

  auto operator==(const Integrator&) const noexcept -> bool = default;
};

struct MockCurve {
  using Scalar = Scalar;

  MOCK_METHOD(Scalar, call, (Scalar v), (const, noexcept));
  MOCK_METHOD(Jet, call, (Jet v), (const, noexcept));
  MOCK_METHOD(Scalar, antiderivative, (Scalar v), (const, noexcept));
  MOCK_METHOD(Jet, antiderivative, (Jet v), (const, noexcept));

  virtual ~MockCurve() = default;
};

// Wraps mock with a curve that requires numerical integration.
struct CurveWithoutAntiderivative {
  using Scalar = MockCurve::Scalar;

  template <typename Value>
  auto operator()(Value v) const noexcept -> Value {
    return mock->call(v);
  }

  auto operator==(const CurveWithoutAntiderivative&) const noexcept
      -> bool = default;

  MockCurve* mock = nullptr;
};

// Wraps mock with a curve supporting .antiderivative().
struct CurveWithAntiderivative {
  using Scalar = MockCurve::Scalar;

  template <typename Value>
  auto operator()(Value v) const noexcept -> Value {
    return mock->call(v);
  }

  template <typename Value>
  auto antiderivative(Value v) const noexcept -> Value {
    return mock->antiderivative(v);
  }

  MockCurve* mock = nullptr;
};

// Lightweight result of building a cached integral.
struct FakeCachedIntegral {
  using Scalar = MockCurve::Scalar;

  template <typename Value>
  auto operator()(Value v) const noexcept -> Value {
    return curve(v);
  }

  CurveWithoutAntiderivative curve;
  Integrator integrator;
};

// ============================================================================
// Antiderivative
// ============================================================================

struct AntiderivativeTest : Test {
  using Scalar = MockCurve::Scalar;

  MockCurve mock_curve;

  const Scalar v_scalar = 3.1;
  const Scalar expected_scalar = 15.2;

  const Jet v_jet{6.7, 1.6};
  const Jet expected_jet{8.9, 17.9};
};

// ----------------------------------------------------------------------------
// Unspecialized
// ----------------------------------------------------------------------------

struct AntiderivativeTestUnspecialized : AntiderivativeTest {
  using Sut = Antiderivative<FakeCachedIntegral>;
  using Curve = CurveWithoutAntiderivative;
  const Sut sut{
      FakeCachedIntegral{.curve = Curve{.mock = &mock_curve}, .integrator = {}},
  };
};

TEST_F(AntiderivativeTestUnspecialized, scalar_invokes_integral_directly) {
  EXPECT_CALL(mock_curve, call(v_scalar)).WillOnce(Return(expected_scalar));

  const auto actual = sut(v_scalar);

  EXPECT_EQ(expected_scalar, actual);
}

TEST_F(AntiderivativeTestUnspecialized, jet_invokes_integral_directly) {
  EXPECT_CALL(mock_curve, call(v_jet)).WillOnce(Return(expected_jet));

  const auto actual = sut(v_jet);

  EXPECT_EQ(expected_jet, actual);
}

// ----------------------------------------------------------------------------
// Specialized
// ----------------------------------------------------------------------------

struct AntiderivativeTestSpecialized : AntiderivativeTest {
  using Sut = Antiderivative<CurveWithAntiderivative>;
  const Sut sut{.curve = CurveWithAntiderivative{&mock_curve}};
};

TEST_F(AntiderivativeTestSpecialized, scalar_calls_member_antiderivative) {
  EXPECT_CALL(mock_curve, antiderivative(v_scalar))
      .WillOnce(Return(expected_scalar));

  const auto actual = sut(v_scalar);

  EXPECT_EQ(expected_scalar, actual);
}

TEST_F(AntiderivativeTestSpecialized, jet_calls_member_antiderivative) {
  EXPECT_CALL(mock_curve, antiderivative(v_jet.a))
      .WillOnce(Return(expected_jet.a));
  EXPECT_CALL(mock_curve, call(v_jet.a)).WillOnce(Return(expected_scalar));

  const auto actual = sut(v_jet);

  EXPECT_EQ(expected_jet.a, actual.a);
  EXPECT_EQ(expected_scalar * v_jet.v, actual.v);
}

// ============================================================================
// AntiderivativeBuilder
// ============================================================================

struct AntiderivativeBuilderTest : Test {
  using Scalar = MockCurve::Scalar;
  static constexpr auto max = Scalar{10};
  static constexpr auto tolerance = Scalar{1e-5};

  using CriticalPoints = std::vector<Scalar>;
  inline static const auto critical_points = CriticalPoints{5, 7, 11};

  using Integral = ComposedIntegral<CurveWithoutAntiderivative, Integrator>;

  struct MockCachedIntegralBuilder {
    MOCK_METHOD(FakeCachedIntegral, call,
                (Integral, Scalar, Scalar, const std::vector<Scalar>&),
                (const));

    auto operator()(Integral integral, Scalar max, Scalar tolerance,
                    const auto& critical_points) const -> FakeCachedIntegral {
      return call(std::move(integral), max, tolerance, critical_points);
    }
  };

  const Integrator expected_integrator{.identity =
                                           Integrator::expected_identity};

  using Sut = AntiderivativeBuilder<MockCachedIntegralBuilder, Integrator>;
  Sut sut{.cached_integral_builder{}, .integrator = expected_integrator};

  MockCurve mock_curve;
};

TEST_F(AntiderivativeBuilderTest, unspecialized_creates_cached_integral) {
  using Curve = CurveWithoutAntiderivative;
  const auto expected_curve = Curve{.mock = &mock_curve};
  EXPECT_CALL(sut.cached_integral_builder,
              call(AllOf(Field(&Integral::integrand, Eq(expected_curve)),
                         Field(&Integral::integrator, Eq(expected_integrator))),
                   DoubleEq(max), DoubleEq(tolerance), Eq(critical_points)))
      .WillOnce(Return(FakeCachedIntegral{.curve = expected_curve,
                                          .integrator = expected_integrator}));

  const auto result = sut(expected_curve, max, tolerance, critical_points);

  ASSERT_EQ(expected_curve, result.curve);
  ASSERT_EQ(expected_integrator, result.integrator);
}

TEST_F(AntiderivativeBuilderTest, specialized_moves_original_curve) {
  const auto result = sut(CurveWithAntiderivative{&mock_curve}, max, tolerance,
                          critical_points);
  ASSERT_EQ(&mock_curve, result.curve.mock);
}

}  // namespace
}  // namespace curves
