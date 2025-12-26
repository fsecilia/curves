// SPDX-License-Identifier: MIT
/*!
  \file
  \brief User mode additions to the kernel spline module.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

extern "C" {
#include <curves/driver/spline.h>
}  // extern "C"

#include <curves/lib.hpp>
#include <curves/math/curve.hpp>
#include <curves/math/fixed.hpp>
#include <algorithm>
#include <ostream>
#include <ranges>

namespace curves {
namespace spline {

// Origin must be below octave 0.
static_assert(SPLINE_SEGMENTS_PER_OCTAVE_LOG2 <= SPLINE_DOMAIN_MIN_SHIFT);

// Imports from c.
auto calc_t(s64 x, int width_log2) -> s64;
auto locate_segment(s64 x, s64* segment_index, s64* t) noexcept -> void;
auto transform_v_to_x(const struct curves_spline* spline, s64 v) noexcept
    -> s64;
auto eval(const struct curves_spline_segment* segment, s64 t) noexcept -> s64;
auto eval(const struct curves_spline* spline, s64 v) noexcept -> s64;

// Knot to form cubic hermite splines, {x, y, dy/dx}.
struct Knot {
  real_t v;
  real_t y;
  real_t m;

  friend auto operator<<(std::ostream& out, const Knot& src) -> std::ostream& {
    return out << src.v << ", " << src.y << ", " << src.m;
  }
};

/*
  Converts from Hermite form in floating-point:
    H(t) = (2t^3 - 3t^2 + 1)y0 + (t^3 - 2t^2 + t)m0
         + (-2t^3 + 3t^2)y1 + (t^3 - t^2)m1

  To monomial form in fixed-point:
    P(t) = at^3 + bt^2 + ct + d
*/
class SegmentConverter {
 public:
  auto operator()(const Knot& k0, const Knot& k1, real_t dv,
                  const curves_spline_segment& prev) const noexcept
      -> curves_spline_segment {
    const auto dy = k1.y - k0.y;
    const auto m0 = k0.m * dv;
    const auto m1 = k1.m * dv;

    auto* c = prev.coeffs;

    // Calc d for the next segment as *exact* sum of rounded terms of the
    // current segment.
    const auto d = c[0] + c[1] + c[2] + c[3];

    return {.coeffs = {Fixed{-2 * dy + m0 + m1}.raw,
                       Fixed{3 * dy - 2 * m0 - m1}.raw, Fixed{m0}.raw, d}};
  }
};

// Encapsulates how knots are located.
struct KnotLocator {
  // Calculates sample location for a given knot index.
  static s64 locate_knot(int knot) {
    int octave, octave_segment_width_log2, segment_within_octave;
    s64 segment;

    // Check for knot 0.
    if (!knot)
      // Sample location of knot 0 is 0.
      return 0;

    // Check for subnormal zone.
    if (knot < SPLINE_SEGMENTS_PER_OCTAVE) {
      // This zone covers [0, DOMAIN_MIN) and exists to extend the
      // geometric indexing scheme all the way to zero with uniform
      // resolution. All segments here have minimum width.
      return (s64)knot << SPLINE_MIN_SEGMENT_WIDTH_LOG2;
    }

    // Determine octave containing knot.
    //
    // After the subnormal zone, knots are grouped into octaves of
    // SEGMENTS_PER_OCTAVE knots each. The value is contained in the high
    // bits, then we subtract out the subnormal zone indices.
    octave = (knot >> SPLINE_SEGMENTS_PER_OCTAVE_LOG2) - 1;

    // Locate segment containing knot relative to current octave.
    segment_within_octave = knot & (SPLINE_SEGMENTS_PER_OCTAVE - 1);

    // Locate segment containing knot globally.
    //
    // The sum total size of all previous octaves is the same as the
    // current octave size, so we offset the global location by 1 octave's
    // worth of segments.
    segment = SPLINE_SEGMENTS_PER_OCTAVE | segment_within_octave;

    // Determine width of segment in octave.
    //
    // Segments in octave 0 have min width; width doubles per octave after.
    octave_segment_width_log2 = SPLINE_MIN_SEGMENT_WIDTH_LOG2 + octave;

    return segment << octave_segment_width_log2;
  }

  auto operator()(int i) const noexcept -> s64 {
    // Call out to shared c implementation.
    return locate_knot(i);
  }

  /*!
    Finds the knot index that is numerically closest to the target velocity.
  */
  auto find_nearest_knot(real_t v) const noexcept -> int {
    const auto count = SPLINE_NUM_SEGMENTS;
    const auto indices = std::views::iota(0, count);

    // Project index -> knot location in reference space.
    const auto project = [&](int index) {
      return Fixed::from_raw((*this)(index)).to_real();
    };

    /*
      Find first x not smaller than v.

      Since x and v are in different domains, this is comparing apples to
      oranges, but it's to find which apple is most similar to which orange,
      so the comparison is useful.
    */
    const auto lower_bound = std::ranges::lower_bound(indices, v, {}, project);

    // Handle boundary conditions.
    if (lower_bound == indices.begin()) return 0;
    if (lower_bound == indices.end()) return count - 1;

    /*
      Find x closest to v.

      The transition between v < x and v >= x happens between lower_bound and
      the iterator before it, so check both.
    */
    const auto next_x = project(*lower_bound);
    const auto prev_x = project(*lower_bound - 1);
    return std::abs(prev_x - v) < std::abs(next_x - v) ? *lower_bound - 1
                                                       : *lower_bound;
  }

  auto find_nearest_x(real_t v) const noexcept -> real_t {
    return Fixed::from_raw(locate_knot(find_nearest_knot(v))).to_real();
  }
};

// Samples a curve to create a knot.
template <typename KnotLocator = KnotLocator>
class KnotSampler {
 public:
  explicit KnotSampler(KnotLocator locator) noexcept
      : locator_{std::move(locator)} {}

  KnotSampler() = default;

  /*
    Samples curve at a specific physical location.

    \param v sample location in physical space, as velocity.
    \returns Knot at sampled location.
  */
  auto operator()(auto&& curve, real_t sensitivity, real_t v) const -> Knot {
    const auto [f, df] = std::forward<decltype(curve)>(curve)(v);
    return {v, f * sensitivity, df * sensitivity};
  }

 private:
  KnotLocator locator_;
};

/*
  Builds a spline by sampling a curve for knots, then building segments
  between the knots.
*/
template <int_t num_segments, typename KnotLocator = KnotLocator,
          typename KnotSampler = KnotSampler<>,
          typename SegmentConverter = SegmentConverter>
class SplineBuilder {
 public:
  SplineBuilder(KnotLocator knot_locator, KnotSampler knot_sampler,
                SegmentConverter segment_converter) noexcept
      : knot_locator_{std::move(knot_locator)},
        knot_sampler_{std::move(knot_sampler)},
        segment_converter_{std::move(segment_converter)} {}

  SplineBuilder() = default;

  auto operator()(auto&& curve, real_t sensitivity) const noexcept
      -> curves_spline {
    curves_spline result;

    auto x_to_v = 1.0L;
    if constexpr (HasCusp<decltype(curve)>) {
      const auto cusp_v = curve.cusp_location();
      const auto cusp_x = knot_locator_.find_nearest_x(cusp_v);
      x_to_v = cusp_v / cusp_x;
      result.v_to_x = Fixed{cusp_x / cusp_v}.raw;
    } else {
      result.v_to_x = Fixed{1}.raw;
    }

    // Start at index 0.
    s64 x0_fixed = knot_locator_(0);
    real_t x0_ref = Fixed::from_raw(x0_fixed).to_real();
    real_t v0 = x0_ref * x_to_v;

    // Pass physical v0 to sampler.
    auto k0 =
        knot_sampler_(std::forward<decltype(curve)>(curve), sensitivity, v0);
    auto prev_segment = curves_spline_segment{};
    for (auto i = 0; i < num_segments; ++i) {
      // Calculate next position.
      const s64 x1_fixed = knot_locator_(i + 1);
      const real_t x1_ref = Fixed::from_raw(x1_fixed).to_real();
      const real_t v1 = x1_ref * x_to_v;

      // Sample next knot.
      const auto k1 =
          knot_sampler_(std::forward<decltype(curve)>(curve), sensitivity, v1);

      // Calculate physical width, dv
      // We still use the integer difference, dx, for precision.
      const s64 width_fixed = x1_fixed - x0_fixed;
      const real_t dx_ref = Fixed::from_raw(width_fixed).to_real();

      // dv = dx * scalar
      const real_t dv = dx_ref * x_to_v;

      // Converter uses knots and physical width.dd
      auto& segment = result.segments[i];
      segment = segment_converter_(k0, k1, dv, prev_segment);
      prev_segment = segment;

      // Advance.
      k0 = k1;
      x0_fixed = x1_fixed;
    }

    result.x_geometric_limit = x0_fixed;

    construct_runout_segment(x_to_v, result.segments[num_segments - 1], result);

    return result;
  }

 private:
  KnotLocator knot_locator_;
  KnotSampler knot_sampler_;
  SegmentConverter segment_converter_;

  /*
    Bleeds off curvature before straightening out so final tangent can be
    linearly extended without a kink in gain when evaluating beyond the
    final segment.

    For some curve parameterizations, this can be very difficult. We have to
    handle curves that cross the boundary with negative gain without the
    bleed-out segment going below zero. Since the max curvature it can bleed
    off over distance is a function of that distance, we use an adaptive
    strategy.

    If we notice the final value has negative slope, we cut the width of the
    final segment in half, giving it the same amount of curvature to cover in
    a shorter distance. This means it can curve more overall locally, so it
    pulls out of a dive sooner. If it still can't pull up fast enough, we
    halve the width again. We keep doing this until it pulls out of the dive,
    or hits the minimum segment width, which is very small. This adaptation
    tries to maintain y''(1) = 0.

    If we get through all loop iterations and still didn't pull up in time, we
    force a hard corner with y'(1) = 0.
  */
  void construct_runout_segment(real_t x_to_v,
                                const curves_spline_segment& prev,
                                curves_spline& result) const noexcept {
    // Establish baselines.
    const int last_idx = num_segments - 1;
    const int prev_width_log2 =
        SPLINE_MIN_SEGMENT_WIDTH_LOG2 +
        ((last_idx >> SPLINE_SEGMENTS_PER_OCTAVE_LOG2) - 1);

    // Fetch previous coefficients for continuity.
    const auto prev_a = Fixed::from_raw(prev.coeffs[0]).to_real();
    const auto prev_b = Fixed::from_raw(prev.coeffs[1]).to_real();
    const auto prev_c = Fixed::from_raw(prev.coeffs[2]).to_real();
    const auto prev_d = Fixed::from_raw(prev.coeffs[3]).to_real();

    const auto y_start = prev_a + prev_b + prev_c + prev_d;

    // Calculate normalized derivatives at the boundary.
    const auto m_start_norm = 3.0 * prev_a + 2.0 * prev_b + prev_c;
    const auto k_start_norm = 6.0 * prev_a + 2.0 * prev_b;

    // Get previous physical width (dv_prev) to un-normalize.
    const s64 prev_len_fixed = 1ULL << prev_width_log2;
    const real_t dv_prev = Fixed::from_raw(prev_len_fixed).to_real() * x_to_v;

    // real slope, gain
    const auto m_real = m_start_norm / dv_prev;

    // real curvature, gain slope
    const auto k_real = k_start_norm / (dv_prev * dv_prev);

    // Start with the ideal "1 Octave" runout.
    const int max_log2 = prev_width_log2 + SPLINE_SEGMENTS_PER_OCTAVE_LOG2;
    int runout_log2 = max_log2;

    // Track whether we found a safe parameterization and the diminishing
    // width.
    bool is_safe = false;
    real_t dv_final = 0;

    // Safety floor nominally 0.
    const real_t kMinSlope = 0.0;

    // Adaptive loop.
    //
    // Search for the widest smooth segment that doesn't dive.
    for (; runout_log2 >= SPLINE_MIN_SEGMENT_WIDTH_LOG2; --runout_log2) {
      // Calculate candidate width.
      const s64 runout_len_fixed = 1ULL << runout_log2;
      dv_final = Fixed::from_raw(runout_len_fixed).to_real() * x_to_v;

      // Predict end slope (assuming standard y''(1)=0 constraint)
      // slope_end = slope_start + 0.5*curvature_start*width
      const real_t projected_slope = m_real + 0.5 * k_real * dv_final;

      if (projected_slope >= kMinSlope) {
        is_safe = true;
        break;
      }
    }

    // Panic fallback.
    //
    // If we exited the loop and are still unsafe, we are at min segment width
    // and the standard logic still results in a negative slope. Clamp to
    // exactly minimum width.
    if (!is_safe) {
      // Force use of minimum width
      runout_log2 = SPLINE_MIN_SEGMENT_WIDTH_LOG2;
      const s64 min_len = 1ULL << runout_log2;
      dv_final = Fixed::from_raw(min_len).to_real() * x_to_v;
    }

    // Commit runout length.
    result.runout_width_log2 = runout_log2;
    result.x_runout_limit = result.x_geometric_limit + (1ULL << runout_log2);

    // Calc constant segment coefficients.
    //
    // These don't change with the constraint. We just have to normalize them
    // to the final chosen width, dv_final.
    const auto next_d = y_start;
    const auto next_c = m_real * dv_final;
    const auto next_b = (k_real * dv_final * dv_final) / 2.0;

    // Calc cubic segment coefficient.
    real_t next_a;
    if (is_safe) {
      // We found a safe width that pulls out of the dive and ends with zero
      // curvature (y'' = 0). This is a smooth bleed-off and it maintains C2
      // continuity.
      next_a = -next_b / 3.0;
    } else {
      // [PANIC MODE]
      // We did not find a safe width that pulls out ofthe dive.
      // Force the curve to flatten. Breaks C2 with a jerk spike, but
      // preserves monotonicity. Derived from 3a + 2b + c = 0, when y' = 0.
      next_a = -(2.0 * next_b + next_c) / 3.0;
    }

    result.runout_segment.coeffs[0] = Fixed{next_a}.raw;
    result.runout_segment.coeffs[1] = Fixed{next_b}.raw;
    result.runout_segment.coeffs[2] = Fixed{next_c}.raw;
    result.runout_segment.coeffs[3] = Fixed{next_d}.raw;
  }
};

inline auto create_spline(auto&& curve, real_t sensitivity) noexcept
    -> curves_spline {
  return SplineBuilder<SPLINE_NUM_SEGMENTS, KnotLocator, KnotSampler<>,
                       SegmentConverter>{}(std::forward<decltype(curve)>(curve),
                                           sensitivity);
}

}  // namespace spline
}  // namespace curves
