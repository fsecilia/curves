// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2026 Frank Secilia
*/

#include "adaptive_subdivider.hpp"
#include <curves/testing/test.hpp>
#include <curves/math/curves/spline/subdivision/error_candidate_locator.hpp>
#include <curves/math/curves/spline/subdivision/sampled_error_estimator.hpp>
#include <cmath>
#include <numbers>

namespace curves {
namespace {

// ============================================================================
// Test Curves
// ============================================================================

//! Linear curve: f(v) = m*v + b
template <typename ScalarType>
struct LinearCurve {
  using Scalar = ScalarType;
  Scalar m = 1.0;
  Scalar b = 0.0;

  template <typename Value>
  auto operator()(const Value& v) const noexcept -> Value {
    return m * v + b;
  }
};

//! Quadratic curve: f(v) = a*v^2 + b*v + c
template <typename ScalarType>
struct QuadraticCurve {
  using Scalar = ScalarType;
  Scalar a = 1.0;
  Scalar b = 0.0;
  Scalar c = 0.0;

  template <typename Value>
  auto operator()(const Value& v) const noexcept -> Value {
    return (a * v + b) * v + c;
  }
};

//! Cubic curve: f(v) = a*v^3 + b*v^2 + c*v + d
template <typename ScalarType>
struct CubicCurve {
  using Scalar = ScalarType;
  Scalar a = 1.0;
  Scalar b = 0.0;
  Scalar c = 0.0;
  Scalar d = 0.0;

  template <typename Value>
  auto operator()(const Value& v) const noexcept -> Value {
    return ((a * v + b) * v + c) * v + d;
  }
};

/*!
  Trigonometric curve: f(v) = 2 - cos(v)

  Defaults to just under a quarter wave over 2*pi to keep it increasing and
  positive.
*/
template <typename ScalarType>
struct TrigCurve {
  using Scalar = ScalarType;
  Scalar frequency = 0.24;  // Just under a quarter.

  template <typename Value>
  auto operator()(const Value& v) const noexcept -> Value {
    using math::cos;
    const auto arg = frequency * v;
    assert(arg <= std::numbers::pi_v<Scalar> / 2);
    return Scalar{2} - cos(arg);
  }
};

/*!
  Power law curve: f(v) = v^y

  y < 1 creates a curve that's  steep near zero and flattens out.
  y > 1 does the opposite.
  A corner at v=0 when y < 1 is particularly difficult for subdivision.
*/
template <typename ScalarType>
struct PowerLawCurve {
  using Scalar = ScalarType;
  Scalar gamma = 0.5;
  Scalar epsilon = 1e-10;

  template <typename Value>
  auto operator()(const Value& v) const noexcept -> Value {
    using math::max;
    using math::pow;

    // Clamp to epsilon to avoid singularity.
    return pow(max(v, epsilon), gamma);
  }
};

/*!
  Step-like curve with smooth transition: f(v) = (tanh(k*(v - v0)) + 1)/2

  Creates a sigmoid shape that transitions from 0 to 1 around v0.
  Higher k means sharper transition, requiring more segments to approximate.
*/
template <typename ScalarType>
struct SigmoidCurve {
  using Scalar = ScalarType;
  Scalar steepness = 10.0;  // k
  Scalar center = 0.5;      // v0

  template <typename Value>
  auto operator()(const Value& v) const noexcept -> Value {
    using math::tanh;

    const auto arg = steepness * (v - center);
    return (tanh(arg) + 1.0) * 0.5;
  }
};

// ============================================================================
// Error Estimator
// ============================================================================

using SegmentErrorEstimate = SegmentErrorEstimate<real_t>;

/*!
  An error estimator test double that returns controlled values for testing the
  subdivider's decision logic independent of actual error calculation.
*/
struct StubErrorEstimator {
  using Result = SegmentErrorEstimate;
  using ErrorFunc = std::function<Result(real_t, real_t)>;

  ErrorFunc error_func;

  // Default: constant small error. Accepts everything immediately.
  StubErrorEstimator()
      : error_func{[](real_t v_max_err, real_t max_err) {
          return Result{.v = 1e-10, .error = v_max_err + max_err / 2};
        }} {}

  explicit StubErrorEstimator(ErrorFunc func) : error_func{std::move(func)} {}

  template <typename Curve, typename Poly>
  auto operator()(const Curve&, const Poly&, real_t v_start,
                  real_t width) const noexcept -> Result {
    return error_func(v_start, width);
  }
};

// ============================================================================
// Test Fixture
// ============================================================================

struct AdaptiveSubdividerTest : Test {
  using Scalar = real_t;

  // Default configuration for most tests.
  SubdivisionConfig config{
      .segments_max = 64,
      .segment_width_min = 1e-6,
      .error_tolerance = 1e-8,
  };

  // Simple domain [0, 1].
  std::array<Scalar, 2> unit_domain{0.0, 1.0};

  // Helper to create a real error estimator.
  auto make_real_estimator() const {
    return SampledErrorEstimator{ErrorCandidateLocator<Scalar>{}};
  }
};

// ============================================================================
// Canary Tests: Exact Representation
// ============================================================================

struct ExactRepresentationTest : AdaptiveSubdividerTest {};

TEST_F(ExactRepresentationTest, LinearCurveProducesSingleSegment) {
  // A linear function is exactly representable by one Hermite segment.
  const auto curve = LinearCurve<Scalar>{.m = 2.5, .b = 1.0};
  const auto subdivider =
      make_adaptive_subdivider(make_real_estimator(), config);

  const auto result = subdivider(curve, unit_domain);

  // Should produce exactly one segment.
  EXPECT_EQ(result.segment_count(), 1);

  // Knots should be the domain endpoints (quantized).
  EXPECT_EQ(result.knots.size(), 2u);
  EXPECT_NEAR(result.knots[0], 0.0, 1e-7);
  EXPECT_NEAR(result.knots[1], 1.0, 1e-7);
}

TEST_F(ExactRepresentationTest, QuadraticCurveProducesSingleSegment) {
  const auto curve = QuadraticCurve<Scalar>{.a = 1.0, .b = 2.0, .c = 1.0};
  const auto subdivider =
      make_adaptive_subdivider(make_real_estimator(), config);

  const auto result = subdivider(curve, unit_domain);

  EXPECT_EQ(result.segment_count(), 1);
}

TEST_F(ExactRepresentationTest, CubicCurveProducesSingleSegment) {
  const auto curve = CubicCurve<Scalar>{.a = 1.0, .b = 0.0, .c = 1.0, .d = 0.0};
  const auto subdivider =
      make_adaptive_subdivider(make_real_estimator(), config);

  const auto result = subdivider(curve, unit_domain);

  EXPECT_EQ(result.segment_count(), 1);
}

// ============================================================================
// Subdivision Behavior Tests
// ============================================================================

struct SubdivisionBehaviorTest : AdaptiveSubdividerTest {};

TEST_F(SubdivisionBehaviorTest, TrigCurveRequiresMultipleSegments) {
  const auto curve = TrigCurve<Scalar>{};
  const auto domain = std::array{Scalar{0.0}, std::numbers::pi_v<Scalar>};

  const auto subdivider =
      make_adaptive_subdivider(make_real_estimator(), config);

  const auto result = subdivider(curve, domain);

  // Should need more than one segment.
  EXPECT_GT(result.segment_count(), 1);

  // But shouldn't hit the maximum (cos is smooth).
  EXPECT_LT(result.segment_count(), config.segments_max);
}

TEST_F(SubdivisionBehaviorTest, HigherFrequencyNeedsMoreSegments) {
  const auto high_freq = TrigCurve<Scalar>{};
  const auto low_freq = TrigCurve<Scalar>{.frequency = high_freq.frequency / 4};
  const auto domain = std::array{Scalar{0.0}, std::numbers::pi_v<Scalar>};

  const auto subdivider =
      make_adaptive_subdivider(make_real_estimator(), config);

  const auto result_low = subdivider(low_freq, domain);
  const auto result_high = subdivider(high_freq, domain);

  // Higher frequency should require more segments.
  EXPECT_GT(result_high.segment_count(), result_low.segment_count());
}

TEST_F(SubdivisionBehaviorTest, TighterToleranceNeedsMoreSegments) {
  const auto curve = TrigCurve<Scalar>{};
  const auto domain = std::array{Scalar{0.0}, std::numbers::pi_v<Scalar>};

  auto loose_config = config;
  loose_config.error_tolerance = 1e-4;

  auto tight_config = config;
  tight_config.error_tolerance = 1e-10;

  const auto loose_subdivider =
      make_adaptive_subdivider(make_real_estimator(), loose_config);
  const auto tight_subdivider =
      make_adaptive_subdivider(make_real_estimator(), tight_config);

  const auto result_loose = loose_subdivider(curve, domain);
  const auto result_tight = tight_subdivider(curve, domain);

  EXPECT_GT(result_tight.segment_count(), result_loose.segment_count());
}

// ============================================================================
// Capacity and Limits Tests
// ============================================================================

struct CapacityTest : AdaptiveSubdividerTest {};

TEST_F(CapacityTest, RespectsSegmentLimit) {
  const auto curve = TrigCurve<Scalar>{};
  const auto domain = std::array{Scalar{0.0}, std::numbers::pi_v<Scalar>};

  // Use very low tolerance but limit max segments.
  auto limited_config = config;
  limited_config.segments_max = 8;
  limited_config.error_tolerance = 1e-15;

  const auto subdivider =
      make_adaptive_subdivider(make_real_estimator(), limited_config);

  const auto result = subdivider(curve, domain);

  // Should not exceed the limit.
  EXPECT_LE(result.segment_count(), limited_config.segments_max);
}

TEST_F(CapacityTest, RespectsMinimumWidth) {
  // Make estimator alwaysreports high error to force maximum subdivision.
  const auto always_bad = StubErrorEstimator{[](real_t v, real_t w) {
    return SegmentErrorEstimate(1.0, v + w / 2);  // error 1.0 at midpoint
  }};

  auto narrow_config = config;
  narrow_config.segment_width_min = 0.1;  // Can't go narrower than 0.1
  narrow_config.segments_max = 1000;  // High limit so width is the constraint

  const auto curve = LinearCurve<Scalar>{};
  const auto subdivider = make_adaptive_subdivider(always_bad, narrow_config);

  const auto result = subdivider(curve, unit_domain);

  // All segments should be at least min_width.
  for (auto i = 0; i < result.segment_count(); ++i) {
    const auto width = result.knots[i + 1] - result.knots[i];
    EXPECT_GE(width, narrow_config.segment_width_min - 1e-10)
        << "Segment " << i << " is too narrow: " << width;
  }
}

// ============================================================================
// Structural Invariant Tests
// ============================================================================

struct StructuralInvariantTest : AdaptiveSubdividerTest {};

TEST_F(StructuralInvariantTest, KnotsAreStrictlyIncreasing) {
  const auto curve = TrigCurve<Scalar>{};
  const auto domain = std::array{Scalar{0.0}, std::numbers::pi_v<Scalar>};

  const auto subdivider =
      make_adaptive_subdivider(make_real_estimator(), config);

  const auto result = subdivider(curve, domain);

  for (auto i = 1u; i < result.knots.size(); ++i) {
    EXPECT_GT(result.knots[i], result.knots[i - 1])
        << "Knots not strictly increasing at index " << i;
  }
}

TEST_F(StructuralInvariantTest, DomainEndpointsPreserved) {
  const auto curve = TrigCurve<Scalar>{.frequency = 2.0};
  const auto v_start = Scalar{0.25};
  const auto v_end =
      std::numbers::pi_v<Scalar> / (2 * curve.frequency + 1) - 0.125;
  const auto domain = std::vector<Scalar>{v_start, v_end};

  const auto subdivider =
      make_adaptive_subdivider(make_real_estimator(), config);

  const auto result = subdivider(curve, domain);

  // First knot should be quantized v_start.
  EXPECT_NEAR(result.knots.front(), quantize::knot_position(v_start), 1e-10);

  // Last knot should be quantized v_end.
  EXPECT_NEAR(result.knots.back(), quantize::knot_position(v_end), 1e-10);
}

TEST_F(StructuralInvariantTest, CriticalPointsBecomeKnots) {
  const auto curve = TrigCurve<Scalar>{};

  // Critical points at 0, 1, 2, 3.
  const auto step =
      0.25 * std::numbers::pi_v<Scalar> / (2 * curve.frequency + 1);
  const auto critical_points =
      std::array{0.0 * step, 1.0 * step, 2.0 * step, 3.0 * step};

  const auto subdivider =
      make_adaptive_subdivider(make_real_estimator(), config);

  const auto result = subdivider(curve, critical_points);

  // Each critical point (quantized) should appear in the knot sequence.
  for (const auto critical_point : critical_points) {
    const auto quantized_cp = quantize::knot_position(critical_point);
    const auto found = std::find_if(
        result.knots.begin(), result.knots.end(),
        [&](Scalar k) { return std::abs(k - quantized_cp) < 1e-10; });

    EXPECT_NE(found, result.knots.end())
        << "Critical point " << critical_point
        << " (quantized: " << quantized_cp << ") not found in knots";
  }
}

TEST_F(StructuralInvariantTest, KnotsAreQuantized) {
  const auto curve = TrigCurve<Scalar>{};
  const auto domain = std::array{Scalar{0.0}, std::numbers::pi_v<Scalar>};

  const auto subdivider =
      make_adaptive_subdivider(make_real_estimator(), config);

  const auto result = subdivider(curve, domain);

  // Every knot should equal its own quantization.
  for (auto i = 0u; i < result.knots.size(); ++i) {
    const auto knot = result.knots[i];
    const auto requantized = quantize::knot_position(knot);

    EXPECT_EQ(knot, requantized)
        << "Knot " << i << " is not quantized: " << knot
        << " != " << requantized;
  }
}

TEST_F(StructuralInvariantTest, SegmentCountMatchesKnotCount) {
  const auto curve = TrigCurve<Scalar>{};
  const auto domain = std::array{Scalar{0.0}, std::numbers::pi_v<Scalar>};

  const auto subdivider =
      make_adaptive_subdivider(make_real_estimator(), config);

  const auto result = subdivider(curve, domain);

  // n segments require n+1 knots.
  EXPECT_EQ(result.knots.size(), result.polys.size() + 1);
}

// ============================================================================
// Priority Queue Behavior Tests
// ============================================================================

struct PriorityBehaviorTest : AdaptiveSubdividerTest {};

TEST_F(PriorityBehaviorTest, WorstErrorSplitFirst) {
  // Create an estimator where the left half has high error, right half has low.
  // The left half should be split before the right.
  const auto tracking_estimator = StubErrorEstimator{
      [&](real_t v_start, real_t width) -> SegmentErrorEstimate {
        const auto midpoint = v_start + width / 2;

        // Left half of domain [0, 0.5) has high error.
        // Right half [0.5, 1) has low error.
        if (v_start < 0.5) {
          return {0.1, midpoint};  // High error
        } else {
          return {1e-12, midpoint};  // Low error (within tolerance)
        }
      }};

  auto test_config = config;
  test_config.error_tolerance = 1e-9;
  test_config.segments_max = 10;

  const auto curve = LinearCurve<Scalar>{};
  const auto subdivider =
      make_adaptive_subdivider(tracking_estimator, test_config);

  const auto result = subdivider(curve, unit_domain);

  // The right half [0.5, 1] should remain as one segment (low error).
  // The left half [0, 0.5] should be subdivided (high error).
  // So we expect more knots in [0, 0.5] than in [0.5, 1].

  auto knots_in_left = 0;
  auto knots_in_right = 0;
  for (const auto k : result.knots) {
    if (k <= 0.5) ++knots_in_left;
    if (k >= 0.5) ++knots_in_right;
  }

  // Left side should have more subdivisions.
  EXPECT_GT(knots_in_left, knots_in_right);
}

// ============================================================================
// Edge Cases
// ============================================================================

struct EdgeCaseTest : AdaptiveSubdividerTest {};

TEST_F(EdgeCaseTest, MinimalDomainTwoPoints) {
  const auto curve = LinearCurve<Scalar>{};
  const auto minimal_domain = std::array{0.0, 0.001};

  const auto subdivider =
      make_adaptive_subdivider(make_real_estimator(), config);

  const auto result = subdivider(curve, minimal_domain);

  // Should still produce valid output.
  EXPECT_GE(result.segment_count(), 1);
  EXPECT_EQ(result.knots.size(), result.polys.size() + 1);
}

TEST_F(EdgeCaseTest, ManyCriticalPoints) {
  const auto curve = TrigCurve<Scalar>{};

  // Many critical points.
  const auto critical_point_count = 20;
  const auto domain_max = std::numbers::pi_v<Scalar>;
  std::array<Scalar, critical_point_count> critical_points;
  for (auto i = 0; i < critical_point_count; ++i) {
    critical_points[i] =
        domain_max * static_cast<Scalar>(i) / critical_point_count;
  }

  const auto subdivider =
      make_adaptive_subdivider(make_real_estimator(), config);

  const auto result = subdivider(curve, critical_points);

  // All critical points should be present.
  EXPECT_GE(result.knots.size(), critical_points.size());
}

TEST_F(EdgeCaseTest, ZeroWidthCriticalPointsCollapse) {
  const auto curve = LinearCurve<Scalar>{};

  // Two critical points that quantize to the same value.
  const auto quantum = Scalar{1.0} / (1 << 24);
  const auto near_points = std::array{
      Scalar{0.0},
      quantum * 0.3,  // Rounds to 0
      Scalar{1.0},
  };

  const auto subdivider =
      make_adaptive_subdivider(make_real_estimator(), config);

  // This might produce a degenerate segment or skip it.
  // The key is it shouldn't crash or produce invalid output.
  const auto result = subdivider(curve, near_points);

  // Should still have valid structure.
  EXPECT_GE(result.segment_count(), 1);

  // Knots should still be strictly increasing.
  for (auto i = 1u; i < result.knots.size(); ++i) {
    EXPECT_GT(result.knots[i], result.knots[i - 1]) << "Error at segment " << i;
  }
}

// ============================================================================
// Stress Tests
// ============================================================================

struct StressTest : AdaptiveSubdividerTest {};

TEST_F(StressTest, PowerLawWithSmallGamma) {
  // gamma = 0.3 creates a sharp corner near zero. This is challenging because
  // the curvature is very high near v=0, requiring many segments there.

  const auto curve = PowerLawCurve<Scalar>{.gamma = 0.3, .epsilon = 1e-6};
  const auto domain = std::array{1e-4, 10.0};

  auto stress_config = config;
  stress_config.segments_max = 128;
  stress_config.error_tolerance = 1e-6;

  const auto subdivider =
      make_adaptive_subdivider(make_real_estimator(), stress_config);

  const auto result = subdivider(curve, domain);

  // Should produce a valid spline without hitting pathological cases.
  EXPECT_GT(result.segment_count(), 1);
  EXPECT_LE(result.segment_count(), 128);

  // Structure should be valid.
  EXPECT_EQ(result.knots.size(), result.polys.size() + 1);
}

TEST_F(StressTest, SteepSigmoid) {
  // Very steep sigmoid - nearly a step function.
  const auto curve = SigmoidCurve<Scalar>{.steepness = 50.0, .center = 0.5};
  const auto domain = std::array{0.0, 1.0};

  auto stress_config = config;
  stress_config.segments_max = 64;
  stress_config.error_tolerance = 1e-4;

  const auto subdivider =
      make_adaptive_subdivider(make_real_estimator(), stress_config);

  const auto result = subdivider(curve, domain);

  // Most segments should be concentrated around the transition at 0.5.
  auto segments_near_center = 0;
  for (auto i = 0; i < result.segment_count(); ++i) {
    const auto mid = (result.knots[i] + result.knots[i + 1]) / 2;
    if (mid > 0.3 && mid < 0.7) {
      ++segments_near_center;
    }
  }

  // More than half the segments should be near the steep transition.
  EXPECT_GT(segments_near_center, result.segment_count() / 2);
}

}  // namespace
}  // namespace curves
