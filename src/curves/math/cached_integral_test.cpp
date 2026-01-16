// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "cached_integral.hpp"
#include <curves/testing/test.hpp>
#include <curves/math/integration.hpp>

namespace curves {
namespace {

// ============================================================================
// ComposedIntegral
// ============================================================================

struct ComposedIntegralTest : Test {
  static constexpr auto kIntegrandScale = 5.6;
  struct Integrand {
    real_t scale = 0.0;

    auto operator()(real_t left, real_t right) const noexcept -> real_t {
      return scale * (right - left);
    }
  };

  static constexpr auto kIntegratorOffset = 7.9;
  struct Integrator {
    real_t offset = 0.0;

    auto operator()(const Integrand& integrand, real_t left,
                    real_t right) const noexcept -> real_t {
      return offset + integrand(left, right);
    }
  };

  static constexpr auto kRight = 3.4;

  using Sut = ComposedIntegral<Integrand, Integrator>;
  Sut sut = ComposedIntegralFactory{Integrator{kIntegratorOffset}}(
      Integrand{kIntegrandScale});
};

TEST_F(ComposedIntegralTest, integrand) {
  EXPECT_DOUBLE_EQ(kIntegrandScale, sut.integrand().scale);
}

TEST_F(ComposedIntegralTest, integrator) {
  EXPECT_DOUBLE_EQ(kIntegratorOffset, sut.integrator().offset);
}

TEST_F(ComposedIntegralTest, eval_single_value) {
  const auto expected = kIntegratorOffset + kIntegrandScale * kRight;
  EXPECT_DOUBLE_EQ(expected, sut(kRight));
}

TEST_F(ComposedIntegralTest, eval_range) {
  const auto kLeft = 1.2;
  const auto expected = kIntegratorOffset + kIntegrandScale * (kRight - kLeft);
  EXPECT_DOUBLE_EQ(expected, sut(kLeft, kRight));
}

// ============================================================================
// CachedIntegral
// ============================================================================

constexpr auto empty_critical_points = std::array<real_t, 0>{};

struct ScalarFunction {
  std::function<real_t(real_t)> function;
  auto operator()(real_t x) const noexcept -> real_t { return function(x); };
};

// Generic Oracle; uses std::function to hold f(x) and F(x).
struct Oracle {
  std::string name;
  ScalarFunction f;  // Function being integrated.
  ScalarFunction F;  // Analytical antiderivative.

  friend std::ostream& operator<<(std::ostream& os, const Oracle& o) {
    return os << o.name;
  }
};

// ----------------------------------------------------------------------------
// Common Fixture
// ----------------------------------------------------------------------------

struct CachedIntegralTest : Test {
  using Integral = ComposedIntegral<ScalarFunction, Gauss5>;
  using Builder = CachedIntegralBuilder;
  using Sut = CachedIntegral<Integral>;
};

// ----------------------------------------------------------------------------
// Analytic Accuracy
// ----------------------------------------------------------------------------

struct AnalyticTestVector {
  Oracle oracle;
  real_t range_end;
  real_t tolerance;
};

struct CachedIntegralAnalyticTest : CachedIntegralTest,
                                    WithParamInterface<AnalyticTestVector> {
  const ScalarFunction& f = GetParam().oracle.f;
  const ScalarFunction& F = GetParam().oracle.F;
  const real_t range_end = GetParam().range_end;
  const real_t tolerance = GetParam().tolerance;

  Sut cached_integral = Builder{}(Integral{f, Gauss5{}}, range_end, tolerance,
                                  empty_critical_points);

  /*
    Since producing the cache sums across intervals, and each interval is
    calculated to within its own min approximation error, the total expected
    error in a particular interval is the product of the per-interval min
    approximation error and the number of intervals before it. The final
    interval has the largest approximation error, n*e. However, it also
    accumulates floating point error, even with Kahan summation, so fudge it
    by 10x. But that's *still* smaller than doubles can represent in the
    range we test at, so multiply by another 10.

    We need to start testing in ulps.
  */
  const real_t max_error = tolerance * cached_integral.cache().size() * 100;

  const std::array<real_t, 3> test_points{
      range_end * 0.1,
      range_end * 0.5,
      range_end * 0.9,
  };
};

TEST_P(CachedIntegralAnalyticTest, TotalArea) {
  // Test total area.
  const auto expected_total = F(GetParam().range_end) - F(0.0);
  EXPECT_NEAR(cached_integral(range_end), expected_total, max_error);
}

TEST_P(CachedIntegralAnalyticTest, InteriorPoints) {
  for (const auto x : test_points) {
    const auto expected = F(x) - F(0.0);
    const auto actual = cached_integral(x);
    EXPECT_NEAR(expected, actual, max_error) << "Failed at x=" << x;
  }
}

TEST_P(CachedIntegralAnalyticTest, Integral) {
  // Make sure integrand matches original function.
  for (const auto x : test_points) {
    const auto expected = f(x);
    const auto actual = cached_integral.integral().integrand()(x);
    EXPECT_DOUBLE_EQ(expected, actual) << "Failed at x=" << x;
  }
}

// Define Oracles
const Oracle kLinear = {"Linear", {[](real_t x) { return x; }}, {[](real_t x) {
                          return 0.5 * x * x;
                        }}};

const Oracle kCubic = {"Cubic",
                       {[](real_t x) { return x * x * x; }},
                       {[](real_t x) { return 0.25 * x * x * x * x; }}};

const Oracle kCos = {"Cos",
                     {[](real_t x) { return std::cos(x); }},
                     {[](real_t x) { return std::sin(x); }}};

INSTANTIATE_TEST_SUITE_P(StandardFunctions, CachedIntegralAnalyticTest,
                         Values(AnalyticTestVector{kLinear, 10.0, 1e-16},
                                AnalyticTestVector{kCubic, 2.0, 1e-16},
                                AnalyticTestVector{kCos, 6.28, 1e-16}));

// ----------------------------------------------------------------------------
// Singularity Test
// ----------------------------------------------------------------------------

struct CachedIntegralSingularityTest : CachedIntegralTest {
  static constexpr auto end = 1.0;
  static constexpr auto tol = 1.0e-10;

  // f(x) = x^0.3 - Has a singularity in derivative at 0.
  static constexpr auto gamma = 0.3;
  inline static const auto f =
      ScalarFunction{[](real_t x) { return std::pow(x, gamma); }};

  const Sut cached =
      Builder{}(Integral{f, Gauss5{}}, end, tol, empty_critical_points);

  const Sut::Cache::key_container_type& keys = cached.cache().keys();
};

TEST_F(CachedIntegralSingularityTest, NumberOfSubdivisions) {
  // Estimating precisely how many intervals this should subdivide into isn't
  // worth doing right now. We know it's more than 5 and less than 1000. This
  // will catch cases where it fails to produce anything, or overproduces to a
  // few orders of magnitude.
  static const auto expected_arbitrary_min = 5;
  static const auto expected_arbitrary_max = 1000;
  ASSERT_GT(keys.size(), expected_arbitrary_min);
  ASSERT_LT(keys.size(), expected_arbitrary_max);
}

TEST_F(CachedIntegralSingularityTest, Monotonicity) {
  for (size_t i = 0; i < keys.size() - 1; ++i) EXPECT_LT(keys[i], keys[i + 1]);
}

TEST_F(CachedIntegralSingularityTest, Density) {
  // Intervals near 0 should be smaller than intervals near 1.
  const auto first_interval = keys[1] - keys[0];
  const auto last_interval = keys.back() - keys[keys.size() - 2];

  EXPECT_LT(first_interval, last_interval);
}

TEST_F(CachedIntegralSingularityTest, PowerLawSingularityAdaptivity) {
  // Check the literal value at end.
  const auto expected = std::pow(end, gamma + 1) / (gamma + 1);

  const auto actual = cached(end);

  EXPECT_NEAR(actual, expected, tol);
}

// ----------------------------------------------------------------------------
// Critical Points Test
// ----------------------------------------------------------------------------

struct CachedIntegralBehaviorTest : CachedIntegralTest {};

TEST_F(CachedIntegralBehaviorTest, CriticalPointsAreRespected) {
  // Use a simple linear function which won't subdivide much.
  auto f = ScalarFunction{[](double x) { return x; }};

  // Force a split at known location.
  const auto critical_point = 0.555;
  auto cached =
      Builder{}(Integral{f, Gauss5{}}, 1.0, 1e-2, std::array{critical_point});

  const auto& keys = cached.cache().keys();

  // Check if critical_point is an exact key in the cache.
  auto found_critical = false;
  for (auto k : keys) {
    if (std::abs(k - critical_point) < 1e-9) {
      found_critical = true;
      break;
    }
  }

  EXPECT_TRUE(found_critical)
      << "Critical point was not preserved as an interval boundary.";
}

}  // namespace
}  // namespace curves
