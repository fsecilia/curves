// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Construction of shaped splines from transfer functions.

  This module takes a transfer function (produced by FromSensitivity or
  FromGain wrapping a generating curve), shaping parameters, and produces
  a shaped_spline struct ready for the driver.

  The construction pipeline:
    1. Build integral cache (for gain mode random access)
    2. Compute floor sensitivity from T'(0) + offset
    3. Seed required knots (cusps, shaping boundaries)
    4. Adaptively subdivide based on approximation error
    5. Fit cubic Hermite segments with offset baked in
    6. Convert to fixed-point
    7. Build k-ary search index

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

extern "C" {
#include <curves/driver/shaped_spline.h>
}

#include <curves/lib.hpp>
#include <curves/config/curve.hpp>
#include <curves/math/input_shaping_view.hpp>
#include <curves/math/integration.hpp>
#include <curves/math/segment/construction.hpp>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include <iostream>

namespace curves {

// ============================================================================
// Concepts
// ============================================================================

template <typename T>
concept HasAnalyticalGain = requires(const T& curve, real_t x) {
  { curve.gain(x) } -> std::convertible_to<real_t>;  // T'(x) = S(x) + x*S'(x)
};

/*!
  Concept for generating curves (the raw mathematical function).

  Curves provide f(x) and f'(x). The interpretation (sensitivity vs gain)
  determines how this maps to a transfer function.
*/
template <typename T>
concept GeneratingCurve =
    HasAnalyticalGain<T> && requires(const T& curve, real_t x) {
      { curve.value(x) } -> std::convertible_to<real_t>;
      { curve.derivative(x) } -> std::convertible_to<real_t>;
    };

/*!
  Concept for curves that have known cusp locations.

  Cusps are points where the derivative is discontinuous. Placing knots
  exactly at cusps allows perfect fitting on both sides.
*/
template <typename T>
concept HasCuspLocations = requires(const T& curve) {
  { curve.cusp_locations() } -> std::convertible_to<std::vector<real_t>>;
};

// ============================================================================
// Configuration
// ============================================================================

/// Floor and offset configuration in user-facing units.
struct FloorConfig {
  real_t v_width = 0;             ///< Floor duration in velocity units.
  real_t sensitivity_offset = 0;  ///< Vertical shift of entire curve.
};

/// Ceiling configuration.
struct CeilingConfig {
  real_t v_begin = 128;  ///< Where ceiling transition starts.
  real_t v_width = 0;    ///< Ceiling transition width.
};

/// Configuration for the adaptive subdivision.
struct SubdivisionConfig {
  real_t tolerance = 1e-4;  ///< Max approximation error per segment.
  real_t v_max = 128.0;     ///< Domain upper bound.
  int max_segments = SHAPED_SPLINE_MAX_SEGMENTS;
  int error_test_points = 16;  ///< Samples per segment for error check.
  int max_depth = 12;          ///< Max recursion depth.

  // this value is too low
  real_t min_width = (1.0L - 1e-4) / (1 << 16);  ///< Min segment width.

  // this is a hack because inv_width doesn't have enough bits.
  real_t max_width = 16.0;  // Force split if segment wider than this
};

/// Full configuration for shaped spline construction.
struct ShapedSplineConfig {
  FloorConfig floor;
  CeilingConfig ceiling;
  SubdivisionConfig subdivision;
  real_t transition_width = 1.0;  ///< Width of ease-in/ease-out transitions.
};

// ============================================================================
// Gain Integral Cache
// ============================================================================

/*!
  Cached integration for gain-mode transfer functions.

  Precomputes T at grid points using stateful Gaussian quadrature, enabling
  efficient random-access queries. For any v, we look up the nearest grid
  point and integrate only the small remainder.

  This avoids O(n) integration cost per query while maintaining accuracy.
*/
class GainIntegralCache {
 public:
  /*!
    Construct cache by integrating G(φ(t)) from 0 to v_max.

    \param G Function returning T'(x) = G(x) for the original curve.
    \param shaping The input shaping view providing φ(v).
    \param v_max Domain upper bound.
    \param grid_spacing Distance between cached grid points.
  */
  GainIntegralCache(std::function<real_t(real_t)> G,
                    const InputShapingView& shaping, real_t v_max,
                    real_t grid_spacing = 0.1);

  /// Evaluate T_shaped(v) = ∫₀ᵛ G(φ(t)) dt.
  [[nodiscard]] auto T_at(real_t v) const -> real_t;

  /// Evaluate T_shaped(v) and T'_shaped(v) = G(φ(v)).
  [[nodiscard]] auto operator()(real_t v) const
      -> std::tuple<real_t, real_t, real_t>;

 private:
  std::function<real_t(real_t)> G_;
  const InputShapingView* shaping_;
  std::vector<real_t> grid_T_;
  real_t grid_spacing_;
  real_t v_max_;

  auto integrate_segment(real_t a, real_t b) const -> real_t;
};

// ============================================================================
// Shaped Evaluators
// ============================================================================

/// Result of shaped evaluation: T and T'.
struct ShapedEval {
  real_t T;
  real_t dT;
};

/*!
  Evaluator for sensitivity mode: T_shaped(v) = v × S(φ(v)).

  The curve provides S(x). This evaluator computes T = v × S(φ(v)),
  which holds S constant in floor/ceiling regions.
*/
template <GeneratingCurve Curve>
class SensitivityShapedEvaluator {
 public:
  SensitivityShapedEvaluator(const Curve& curve,
                             const InputShapingView& shaping)
      : curve_{curve}, shaping_{shaping} {}

  [[nodiscard]] auto operator()(real_t v) const -> ShapedEval {
    const auto [u, du, d2u] = shaping_(v);

    if (u < kEpsilon) {
      const real_t floor_dT = curve_.value(0);
      return {v * floor_dT, floor_dT};
    }

    const real_t S = curve_.value(u);
    const real_t T_shaped = v * S;

    real_t dT_shaped;
    if constexpr (HasAnalyticalGain<Curve>) {
      // Stable formula: dT = S + (v * du / u) * (G - S)
      const real_t G = curve_.gain(u);
      const real_t R = v * du / u;
      dT_shaped = S + R * (G - S);
    } else {
      // Fallback (may be unstable for curves with S'(0) = ∞)
      const real_t dS = curve_.derivative(u);
      dT_shaped = S + v * dS * du;
    }

    return {T_shaped, dT_shaped};
  }

 private:
  const Curve& curve_;
  const InputShapingView& shaping_;
  static constexpr real_t kEpsilon = 1e-10;
};

/*!
  Evaluator for gain mode: T_shaped(v) = ∫₀ᵛ G(φ(t)) dt.

  The curve provides G(x). Uses GainIntegralCache for efficient
  random-access queries. This holds G constant in floor/ceiling regions.
*/
template <GeneratingCurve Curve>
class GainShapedEvaluator {
 public:
  GainShapedEvaluator(const Curve& curve, const InputShapingView& shaping,
                      real_t v_max, real_t grid_spacing = 0.1)
      : curve_{curve},
        shaping_{shaping},
        cache_{[&curve](real_t x) { return curve.value(x); }, shaping, v_max,
               grid_spacing} {}

  [[nodiscard]] auto operator()(real_t v) const -> ShapedEval {
    const auto [u, T, dT] = cache_(v);

    // Floor region: φ(v) ≈ 0
    // T'(0) = G(0) for gain mode (via L'Hôpital)
    if (u < kEpsilon) {
      const real_t floor_dT = curve_.value(0);
      return {v * floor_dT, floor_dT};
    }

    return {T, dT};
  }

 private:
  const Curve& curve_;
  const InputShapingView& shaping_;
  GainIntegralCache cache_;
  static constexpr real_t kEpsilon = 1e-10;
};

// ============================================================================
// Adaptive Subdivision
// ============================================================================

/// A knot with position and function values.
struct SplineKnot {
  real_t v;
  real_t T;
  real_t dT;
};

/*!
  Adaptive subdivision based on Hermite approximation error.

  Recursively splits intervals where the cubic Hermite interpolant
  deviates from the true function by more than the tolerance.
*/
class AdaptiveSubdivider {
 public:
  using EvalFunc = std::function<ShapedEval(real_t)>;

  AdaptiveSubdivider(EvalFunc eval, const SubdivisionConfig& config)
      : eval_{std::move(eval)}, config_{config} {}

  /*!
    Subdivide starting from required knots.

    \param required_knots Sorted positions that must be knots (boundaries,
                          cusps, transition points).
    \return All knots after subdivision, sorted by position.
  */
  [[nodiscard]] auto subdivide(std::vector<real_t> required_knots)
      -> std::vector<SplineKnot>;

 private:
  EvalFunc eval_;
  SubdivisionConfig config_;

  void subdivide_interval(real_t v0, real_t v1, const SplineKnot& k0,
                          const SplineKnot& k1, int depth,
                          std::vector<SplineKnot>& result);

  [[nodiscard]] auto measure_error(real_t v0, real_t v1, const SplineKnot& k0,
                                   const SplineKnot& k1) const
      -> std::pair<real_t, real_t>;

  [[nodiscard]] static auto hermite_eval(real_t t, real_t y0, real_t y1,
                                         real_t m0, real_t m1, real_t width)
      -> real_t;
};

// ============================================================================
// Helper Functions
// ============================================================================

/// Get cusp locations if available, empty otherwise.
template <typename Curve>
[[nodiscard]] auto get_cusp_locations(const Curve& curve)
    -> std::vector<real_t> {
  if constexpr (HasCuspLocations<Curve>) {
    return curve.cusp_locations();
  } else {
    return {};
  }
}

/// Invert shaping function to find v where φ(v) = u_target.
[[nodiscard]] auto invert_shaping(const InputShapingView& shaping,
                                  real_t u_target) -> std::optional<real_t>;

/// Compute required knot positions from boundaries, transitions, and cusps.
[[nodiscard]] auto compute_required_knots(const InputShapingView& shaping,
                                          const std::vector<real_t>& cusps,
                                          real_t v_max) -> std::vector<real_t>;

// ============================================================================
// Hermite Fitting
// ============================================================================

/// Cubic coefficients: a×t³ + b×t² + c×t + d for t ∈ [0, 1].
struct CubicCoeffs {
  real_t a, b, c, d;
};

/// Convert Hermite endpoint data to cubic coefficients.
[[nodiscard]] auto hermite_to_cubic(const SplineKnot& k0, const SplineKnot& k1)
    -> CubicCoeffs;

// ============================================================================
// Fixed-Point Conversion
// ============================================================================

[[nodiscard]] auto knot_to_fixed(real_t v) -> u32;

// ============================================================================
// k-ary Index Construction
// ============================================================================

/// Build two-level k-ary search index from knot positions.
void build_kary_index(shaped_spline& spline);

// ============================================================================
// Main Builder
// ============================================================================

/*!
  Build a shaped spline from a generating curve.

  \tparam Curve Generating curve type providing value(x) and derivative(x).

  \param curve The generating curve (sensitivity or gain depending on interp).
  \param shaping Input shaping parameters.
  \param interp Whether the curve represents sensitivity or gain.
  \param config Construction configuration.
  \return The shaped_spline ready for the driver.
*/
template <GeneratingCurve Curve>
[[nodiscard]] auto build_shaped_spline(const Curve& curve,
                                       const InputShapingView& shaping,
                                       CurveInterpretation interp,
                                       const ShapedSplineConfig& config = {})
    -> shaped_spline {
  const real_t v_max = config.subdivision.v_max;
  const real_t offset = config.floor.sensitivity_offset;

  // Create evaluator based on interpretation.
  std::function<ShapedEval(real_t)> eval_func;

  if (interp == CurveInterpretation::kSensitivity) {
    auto evaluator =
        std::make_shared<SensitivityShapedEvaluator<Curve>>(curve, shaping);
    eval_func = [evaluator](real_t v) { return (*evaluator)(v); };
  } else {
    auto evaluator =
        std::make_shared<GainShapedEvaluator<Curve>>(curve, shaping, v_max);
    eval_func = [evaluator](real_t v) { return (*evaluator)(v); };
  }

  // Compute required knots.
  auto cusps = get_cusp_locations(curve);
  auto required_knots = compute_required_knots(shaping, cusps, v_max);

#if 0
  for (const auto& knot : required_knots) {
    if (knot < 1e-5) continue;
    static const auto offset_directions = {-1, 0, 1};
    static const auto offset_magnitude = 1e-7;
    for (const auto offset_direction : offset_directions) {
      const auto offset = offset_direction * offset_magnitude;
      const auto x = knot + offset;
      const auto offset_eval = eval_func(x);
      std::cout << "x=" << x << ": {.T=" << offset_eval.T
                << ", .dT=" << offset_eval.dT << "} ";
    }
    std::cout << std::endl;
  }
#endif

  // Adaptive subdivision.
  AdaptiveSubdivider subdivider{eval_func, config.subdivision};
  auto knots = subdivider.subdivide(std::move(required_knots));

  std::cout << "First 3 knots:\n";
  for (int i = 0; i < std::min(3, (int)knots.size()); ++i) {
    std::cout << "  v=" << knots[i].v << " T=" << knots[i].T
              << " dT=" << knots[i].dT << "\n";
  }

  std::cout << "Last 5 knots:\n";
  for (int i = std::max(0, (int)knots.size() - 5); i < (int)knots.size(); ++i) {
    std::cout << "  [" << i << "] v=" << knots[i].v << " T=" << knots[i].T
              << " dT=" << knots[i].dT << "\n";
  }

  // Build the shaped_spline structure.
  shaped_spline result{};

  const int num_segments = static_cast<int>(knots.size()) - 1;
  result.num_segments = static_cast<u16>(num_segments);
  result.v_max = knot_to_fixed(v_max);

  // Convert knots to fixed-point.
  for (int i = 0; i <= num_segments; ++i) {
    result.knots[i] = knot_to_fixed(knots[i].v);
  }

  // Convert segments with offset baked in.
  for (int i = 0; i < num_segments; ++i) {
    const auto& k0 = knots[i];
    const auto& k1 = knots[i + 1];
    auto coeffs = hermite_to_cubic(k0, k1);

    const real_t width = k1.v - k0.v;
    const real_t knot_v = k0.v;

    // Bake offset into coefficients:
    //   T_new(t) = T_orig(t) + offset × (knot + t×width)
    //   c_new = c + offset × width
    //   d_new = d + offset × knot
    coeffs.c += offset * width;
    coeffs.d += offset * knot_v;

    const auto params = segment::SegmentParams{
        .coeffs = {coeffs.a, coeffs.b, coeffs.c, coeffs.d}, .width = width};
    auto normalized = segment::create_segment(params);
    result.packed_segments[i] = segment::pack(normalized);

    if (!i) {
      std::cout << "First segment after baking:\n";
      std::cout << "  width=" << width << " offset=" << offset << "\n";
      std::cout << "  c before=" << (coeffs.c - offset * width)
                << " c after=" << coeffs.c << "\n";
    }
  }

  // Build k-ary index.
  build_kary_index(result);

  return result;
}

}  // namespace curves
