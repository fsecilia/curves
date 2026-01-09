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

using Value = double;

constexpr auto empty_critical_points = std::array<Value, 0>{};

using ScalarFunction = std::function<Value(Value)>;

// Generic Oracle; uses std::function to hold f(x) and F(x).
struct Oracle {
  std::string name;
  ScalarFunction f;  // Function being integrated.
  ScalarFunction F;  // Analytical antiderivative.

  friend std::ostream& operator<<(std::ostream& os, const Oracle& o) {
    return os << o.name;
  }
};

// ============================================================================
// Common Fixture
// ============================================================================

struct CachedIntegralTest : Test {
  using Integral = ComposedIntegral<ScalarFunction, Gauss5>;
  using Builder = CachedIntegralBuilder;
  using Sut = CachedIntegral<Value, Integral>;
};

// ============================================================================
// Analytic Accuracy
// ============================================================================

struct AnalyticTestVector {
  Oracle oracle;
  Value range_end;
  Value tolerance;
};

struct CachedIntegralAnalyticTest : CachedIntegralTest,
                                    WithParamInterface<AnalyticTestVector> {
  Sut cached_integral =
      Builder{}(Integral{GetParam().oracle.f, Gauss5{}}, GetParam().range_end,
                GetParam().tolerance, empty_critical_points);

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
  const Value max_error =
      GetParam().tolerance * cached_integral.cache().size() * 100;
};

TEST_P(CachedIntegralAnalyticTest, TotalArea) {
  const auto& p = GetParam();

  // Test total area.
  const auto expected_total = p.oracle.F(p.range_end) - p.oracle.F(0.0);
  EXPECT_NEAR(cached_integral(p.range_end), expected_total, max_error);
}

TEST_P(CachedIntegralAnalyticTest, InteriorPoints) {
  const auto& p = GetParam();

  const auto test_points =
      std::array{p.range_end * 0.1, p.range_end * 0.5, p.range_end * 0.9};

  for (auto x : test_points) {
    const auto expected = p.oracle.F(x) - p.oracle.F(0.0);
    EXPECT_NEAR(cached_integral(x), expected, max_error) << "Failed at x=" << x;
  }
}

// Define Oracles
const Oracle kLinear = {"Linear", [](Value x) { return x; },
                        [](Value x) { return 0.5 * x * x; }};

const Oracle kCubic = {"Cubic", [](Value x) { return x * x * x; },
                       [](Value x) { return 0.25 * x * x * x * x; }};

const Oracle kCos = {"Cos", [](Value x) { return std::cos(x); },
                     [](Value x) { return std::sin(x); }};

INSTANTIATE_TEST_SUITE_P(StandardFunctions, CachedIntegralAnalyticTest,
                         Values(AnalyticTestVector{kLinear, 10.0, 1e-16},
                                AnalyticTestVector{kCubic, 2.0, 1e-16},
                                AnalyticTestVector{kCos, 6.28, 1e-16}));

// ============================================================================
// Singularity Test
// ============================================================================

struct CachedIntegralSingularityTest : CachedIntegralTest {
  static constexpr auto end = Value{1.0};
  static constexpr auto tol = Value{1e-10};

  // f(x) = x^0.3 - Has a singularity in derivative at 0.
  static constexpr auto gamma = Value{0.3};
  static constexpr auto f = [](Value x) { return std::pow(x, gamma); };

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

// ============================================================================
// Critical Points Test
// ============================================================================

struct CachedIntegralBehaviorTest : CachedIntegralTest {};

TEST_F(CachedIntegralBehaviorTest, CriticalPointsAreRespected) {
  // Use a simple linear function which won't subdivide much.
  auto f = [](double x) { return x; };

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
