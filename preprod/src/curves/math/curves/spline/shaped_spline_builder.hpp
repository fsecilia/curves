// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Builds shaped_spline from a TransferFunction.

  This module takes a TransferFunction (which already composes input shaping
  and the underlying curve with proper Jet propagation) and produces a
  shaped_spline ready for the kernel driver.

  The pipeline is simple:
    1. Adaptive subdivision of the TransferFunction
    2. Pack knots and polynomials into fixed-point format
    3. Build k-ary search index

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

extern "C" {
#include <curves/driver/shaped_spline.h>
}

#include <curves/lib.hpp>
#include <curves/math/curves/spline/segment/construction.hpp>
#include <curves/math/curves/spline/segment/packing.hpp>
#include <curves/math/curves/spline/subdivision/error_candidate_locator.hpp>
#include <curves/math/curves/spline/subdivision/sampled_error_estimator.hpp>
#include <curves/math/curves/spline/subdivision/subdivider.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace curves {

// ============================================================================
// Fixed-Point Conversion
// ============================================================================

namespace detail {

//! Converts a quantized knot position to Q8.24 integer.
inline auto to_q8_24(real_t v) noexcept -> u32 {
  constexpr auto kScale =
      static_cast<real_t>(1ULL << SHAPED_SPLINE_KNOT_FRAC_BITS);
  const auto scaled = v * kScale;

  if (scaled < 0) return 0;
  if (scaled >= static_cast<real_t>(UINT32_MAX)) return UINT32_MAX;

  return static_cast<u32>(std::round(scaled));
}

// ============================================================================
// k-ary Index Construction
// ============================================================================

//! Builds two-level k-ary search index for O(1) average segment lookup.
inline auto build_kary_index(shaped_spline& spline) noexcept -> void {
  const auto n = numeric_cast<int>(spline.num_segments);
  const auto knots = spline.knots;

  // Level 0: 8 separators dividing domain into 9 regions.
  for (auto i = 0; i < SHAPED_SPLINE_KARY_KEYS; ++i) {
    const auto seg = (i + 1) * n / SHAPED_SPLINE_KARY_FANOUT;
    spline.kary_l0[i] = knots[std::min(seg, n)];
  }

  // Level 1: each L0 region subdivided into 9 sub-regions.
  for (auto r0 = 0; r0 < SHAPED_SPLINE_KARY_L1_REGIONS; ++r0) {
    const auto seg_start = r0 * n / SHAPED_SPLINE_KARY_FANOUT;
    const auto seg_end = (r0 + 1) * n / SHAPED_SPLINE_KARY_FANOUT;
    const auto region_size = seg_end - seg_start;

    for (auto i = 0; i < SHAPED_SPLINE_KARY_KEYS; ++i) {
      auto seg = seg_start + (i + 1) * region_size / SHAPED_SPLINE_KARY_FANOUT;
      seg = std::min(seg, n);
      spline.kary_l1[r0][i] = knots[seg];
    }
  }

  // Bucket base indices for final linear scan.
  for (auto r0 = 0; r0 < SHAPED_SPLINE_KARY_L1_REGIONS; ++r0) {
    const auto seg_start = r0 * n / SHAPED_SPLINE_KARY_FANOUT;
    const auto seg_end = (r0 + 1) * n / SHAPED_SPLINE_KARY_FANOUT;
    const auto region_size = seg_end - seg_start;

    for (auto r1 = 0; r1 < SHAPED_SPLINE_KARY_FANOUT; ++r1) {
      const auto bucket = r0 * SHAPED_SPLINE_KARY_FANOUT + r1;
      auto seg = seg_start + r1 * region_size / SHAPED_SPLINE_KARY_FANOUT;
      seg = std::min(seg, n - 1);
      spline.kary_base[bucket] = static_cast<u8>(seg);
    }
  }
}

}  // namespace detail

// ============================================================================
// Builder Configuration
// ============================================================================

struct ShapedSplineConfig {
  int max_segments = SHAPED_SPLINE_MAX_SEGMENTS;
  real_t error_tolerance = 1e-8L;
  real_t min_segment_width = 1.0L / (1 << 16);
  real_t v_max = 128.0L;
};

// ============================================================================
// Builder
// ============================================================================

/*!
  Builds a shaped_spline from a TransferFunction.

  The TransferFunction must:
  - Be callable as (Jet<real_t>) -> Jet<real_t>
  - Expose critical_points(real_t v_max) -> vector<real_t>

  \param transfer_fn The composed transfer function (shaping + curve).
  \param config Build parameters.
  \return shaped_spline ready for the kernel driver.
*/
template <typename TransferFunction>
auto build_shaped_spline(const TransferFunction& transfer_fn,
                         const ShapedSplineConfig& config = {})
    -> shaped_spline {
  // Gather critical points from the transfer function.
  // These propagate up from: UserCurve -> EaseIn -> EaseOut -> ShapedCurve ->
  // TransferFunction Each layer domain-transforms its children's critical
  // points.
  const auto critical_points = transfer_fn.critical_points(config.v_max);

  // Run adaptive subdivision.
  const auto subdiv_config = SubdivisionConfig{
      .segments_max = config.max_segments,
      .segment_width_min = config.min_segment_width,
      .error_tolerance = config.error_tolerance,
  };

  const auto error_estimator = SampledErrorEstimator{ErrorCandidateLocator{}};

  const auto subdivider =
      make_adaptive_subdivider(error_estimator, subdiv_config);
  const auto result = subdivider(transfer_fn, critical_points);

  // Pack into shaped_spline.
  shaped_spline spline{};

  const auto num_segments = result.num_segments();
  spline.num_segments = static_cast<u16>(num_segments);
  spline.v_max = detail::to_q8_24(config.v_max);

  // Pack knots (already quantized to Q8.24 granularity in float).
  for (size_t i = 0; i <= num_segments; ++i) {
    spline.knots[i] = detail::to_q8_24(result.knots[i]);
  }

  // Pack segments.
  for (size_t i = 0; i < num_segments; ++i) {
    const auto width = result.knots[i + 1] - result.knots[i];

    const auto params = segment::SegmentParams{
        .poly = result.polys[i],
        .width = width,
    };

    const auto normalized = segment::create_segment(params);
    spline.packed_segments[i] = segment::pack(normalized);
  }

  // Build k-ary search index.
  detail::build_kary_index(spline);

  return spline;
}

}  // namespace curves
