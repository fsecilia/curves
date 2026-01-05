// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Implementation of shaped spline construction.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#include "shaped_spline_builder.hpp"
#include <curves/numeric_cast.hpp>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

#include <list>
#include <queue>

namespace curves {

// ============================================================================
// Gain Integral Cache
// ============================================================================

GainIntegralCache::GainIntegralCache(std::function<real_t(real_t)> G,
                                     const InputShapingView& shaping,
                                     real_t v_max, real_t grid_spacing)
    : G_{std::move(G)},
      shaping_{&shaping},
      grid_spacing_{grid_spacing},
      v_max_{v_max} {
  // Number of grid points covering [0, v_max].
  const auto num_points = static_cast<std::size_t>(v_max / grid_spacing) + 2;
  grid_T_.reserve(num_points);

  // Stateful integration: accumulate T at each grid point.
  // Monotonic advancement enables efficient gauss5 without redundant work.
  real_t T = 0;
  real_t v_prev = 0;
  grid_T_.push_back(0);  // T(0) = 0

  for (auto i = 1U; i < num_points; ++i) {
    const real_t v = std::min(i * grid_spacing, v_max);
    T += integrate_segment(v_prev, v);
    grid_T_.push_back(T);
    v_prev = v;
  }
}

auto GainIntegralCache::integrate_segment(real_t a, real_t b) const -> real_t {
  // Integrand is G(φ(t)) where G is the original gain function
  // and φ is the shaping function.
  return gauss5([this](real_t t) { return G_(shaping_->eval(t)); }, a, b);
}

auto GainIntegralCache::T_at(real_t v) const -> real_t {
  if (v <= 0) return 0;
  if (v >= v_max_) v = v_max_;

  // Find the grid point at or before v.
  const auto idx = static_cast<std::size_t>(v / grid_spacing_);
  const real_t g = idx * grid_spacing_;
  const real_t T_g = grid_T_[idx];

  // Integrate the remainder [g, v].
  constexpr real_t kEpsilon = 1e-12L;
  if (v - g < kEpsilon) return T_g;

  return T_g + integrate_segment(g, v);
}

auto GainIntegralCache::operator()(real_t v) const
    -> std::tuple<real_t, real_t, real_t> {
  const real_t T = T_at(v);
  const real_t u = shaping_->eval(v);
  const real_t dT = G_(u);  // G(φ(v)) = T'_shaped(v)
  return {u, T, dT};
}

// ============================================================================
// Shaping Inversion
// ============================================================================

auto invert_shaping(const InputShapingView& shaping, real_t u_target)
    -> std::optional<real_t> {
  // The shaping function φ(v) is piecewise:
  //   floor:  φ = 0
  //   trans1: φ = monotonic sextic
  //   linear: φ = v - lag
  //   trans2: φ = monotonic sextic
  //   ceiling: φ = constant

  if (u_target < 0) return std::nullopt;

  const real_t ceiling_val = shaping.ease_out_u_ceiling();
  if (ceiling_val > 0 && u_target > ceiling_val) return std::nullopt;

  // u_target = 0 maps to anywhere in the floor. Return floor start.
  constexpr real_t kEpsilon = 1e-12L;
  if (u_target < kEpsilon) return 0;

  // Get shaping boundaries.
  const real_t v_trans1_begin = shaping.ease_in_transition_v_begin();
  const real_t v_trans1_end = shaping.ease_in_transition_v_end();
  const real_t v_trans2_begin = shaping.ease_out_transition_v_begin();
  const real_t v_trans2_end = shaping.ease_out_transition_v_end();
  const real_t lag = shaping.ease_in_u_lag();

  // Check linear region: φ(v) = v - lag.
  const real_t u_at_trans1_end = shaping.eval(v_trans1_end);
  const real_t u_at_trans2_begin = shaping.eval(v_trans2_begin);

  if (u_target >= u_at_trans1_end && u_target <= u_at_trans2_begin) {
    return u_target + lag;
  }

  // Bisection for transition regions.
  auto bisect = [&](real_t v_lo, real_t v_hi) -> real_t {
    constexpr int kMaxIter = 64;
    for (int i = 0; i < kMaxIter && (v_hi - v_lo) > kEpsilon; ++i) {
      const real_t v_mid = (v_lo + v_hi) / 2;
      const real_t u_mid = shaping.eval(v_mid);
      if (u_mid < u_target) {
        v_lo = v_mid;
      } else {
        v_hi = v_mid;
      }
    }
    return (v_lo + v_hi) / 2;
  };

  // Transition 1?
  if (u_target > 0 && u_target < u_at_trans1_end) {
    return bisect(v_trans1_begin, v_trans1_end);
  }

  // Transition 2?
  if (u_target > u_at_trans2_begin && ceiling_val > 0 &&
      u_target < ceiling_val) {
    return bisect(v_trans2_begin, v_trans2_end);
  }

  // Ceiling region: φ is constant, no unique inverse.
  return std::nullopt;
}

// ============================================================================
// Required Knot Seeding
// ============================================================================

auto compute_required_knots(const InputShapingView& shaping,
                            const std::vector<real_t>& cusps, real_t v_max)
    -> std::vector<real_t> {
  std::vector<real_t> knots;
  knots.reserve(cusps.size() + 8);

  // Domain boundaries.
  knots.push_back(0);
  knots.push_back(v_max);

  // Shaping transition boundaries.
  const real_t v_trans1_begin = shaping.ease_in_transition_v_begin();
  const real_t v_trans1_end = shaping.ease_in_transition_v_end();
  const real_t v_trans2_begin = shaping.ease_out_transition_v_begin();
  const real_t v_trans2_end = shaping.ease_out_transition_v_end();

  auto add_if_valid = [&](real_t v) {
    if (v > 0 && v < v_max) {
      knots.push_back(v);
    }
  };

  add_if_valid(v_trans1_begin);
  add_if_valid(v_trans1_end);
  add_if_valid(v_trans2_begin);
  add_if_valid(v_trans2_end);

  // Cusp locations mapped through shaping inverse.
  for (const real_t u_cusp : cusps) {
    if (auto v_cusp = invert_shaping(shaping, u_cusp)) {
      add_if_valid(*v_cusp);
    }
  }

  // Sort and deduplicate.
  std::sort(knots.begin(), knots.end());
  constexpr real_t kEpsilon = 1e-10L;
  knots.erase(std::unique(knots.begin(), knots.end(),
                          [](real_t a, real_t b) {
                            return std::abs(a - b) < kEpsilon;
                          }),
              knots.end());

  std::cout << "Required knots: ";
  for (auto v : knots) std::cout << v << " ";
  std::cout << "\n";

  return knots;
}

// ============================================================================
// Hermite Interpolation
// ============================================================================

auto AdaptiveSubdivider::hermite_eval(real_t t, real_t y0, real_t y1, real_t m0,
                                      real_t m1, real_t width) -> real_t {
  // Hermite basis:
  //   h00 = 2t³ - 3t² + 1
  //   h10 = t³ - 2t² + t
  //   h01 = -2t³ + 3t²
  //   h11 = t³ - t²
  const real_t t2 = t * t;
  const real_t t3 = t2 * t;

  const real_t h00 = 2 * t3 - 3 * t2 + 1;
  const real_t h10 = t3 - 2 * t2 + t;
  const real_t h01 = -2 * t3 + 3 * t2;
  const real_t h11 = t3 - t2;

  return h00 * y0 + h10 * m0 * width + h01 * y1 + h11 * m1 * width;
}

auto AdaptiveSubdivider::measure_error(real_t v0, real_t v1,
                                       const SplineKnot& k0,
                                       const SplineKnot& k1) const
    -> std::pair<real_t, real_t> {
  real_t max_error = 0;
  real_t worst_v = (v0 + v1) / 2;

  auto coeffs = hermite_to_cubic(k0, k1);
  const real_t width = v1 - v0;
  const real_t inv_width = 1 / width;

  for (int i = 1; i < config_.error_test_points - 1; ++i) {
    const real_t t = static_cast<real_t>(i) / (config_.error_test_points - 1);
    const real_t v = v0 + t * width;

    auto [T_true, dT_true] = eval_(v);

    // Hermite evaluation
    const real_t t2 = t * t;
    const real_t t3 = t2 * t;
    const real_t T_approx =
        coeffs.a * t3 + coeffs.b * t2 + coeffs.c * t + coeffs.d;
    const real_t dT_dt = 3 * coeffs.a * t2 + 2 * coeffs.b * t + coeffs.c;
    const real_t dT_approx = dT_dt * inv_width;

    // Relative errors
    const real_t T_err = std::abs(T_true - T_approx) / T_scale_;
    const real_t dT_err = std::abs(dT_true - dT_approx) / dT_scale_;

    const real_t combined = std::max(T_err, dT_err);

    if (combined > max_error) {
      max_error = combined;
      worst_v = v;
    }
  }

  return {max_error, worst_v};
}

void AdaptiveSubdivider::subdivide_interval(real_t v0, real_t v1,
                                            const SplineKnot& k0,
                                            const SplineKnot& k1, int depth,
                                            std::vector<SplineKnot>& result) {
  const real_t width = v1 - v0;

  // Termination conditions.
  if (width < config_.min_width) return;
  if (depth >= config_.max_depth) return;
  if (numeric_cast<int>(result.size()) >= config_.max_segments) return;

  // Measure error.
  const auto [error, split_pos] = measure_error(v0, v1, k0, k1);
  if (error <= config_.tolerance /*&& width <= config_.max_width*/) return;

  // Don't create segments below minimum width
  if (split_pos - v0 < config_.min_width) return;
  if (v1 - split_pos < config_.min_width) return;

  // Subdivide at max error position.
  const ShapedEval mid_eval = eval_(split_pos);
  const SplineKnot k_mid{split_pos, mid_eval.T, mid_eval.dT};

  // Insert maintaining sorted order.
  auto insert_pos = std::lower_bound(
      result.begin(), result.end(), k_mid,
      [](const SplineKnot& a, const SplineKnot& b) { return a.v < b.v; });
  result.insert(insert_pos, k_mid);

  // Recurse both halves.
  subdivide_interval(v0, split_pos, k0, k_mid, depth + 1, result);
  subdivide_interval(split_pos, v1, k_mid, k1, depth + 1, result);
}

auto AdaptiveSubdivider::subdivide_breadth_first(
    std::vector<real_t> required_knots) -> std::vector<SplineKnot> {
  assert(!required_knots.empty());
  assert(std::is_sorted(required_knots.begin(), required_knots.end()));

  // Initialize with required knots.
  std::vector<SplineKnot> result;
  result.reserve(numeric_cast<std::size_t>(config_.max_segments));

  for (const real_t v : required_knots) {
    const ShapedEval e = eval_(v);
    result.push_back({v, e.T, e.dT});
  }

  // Subdivide each interval.

  bool changed = true;
  while (changed && numeric_cast<int>(result.size()) < config_.max_segments) {
    changed = false;

    // Single pass over all current intervals
    for (size_t i = 0; i + 1 < result.size(); ++i) {
      if (numeric_cast<int>(result.size()) >= config_.max_segments) break;

      const SplineKnot& k0 = result[i];
      const SplineKnot& k1 = result[i + 1];
      // const real_t width = k1.v - k0.v;

      const auto [error, split_pos] = measure_error(k0.v, k1.v, k0, k1);
      if (error <= config_.tolerance /*&& width <= config_.max_width*/)
        continue;

      // Insert one knot (no recursion)
      const ShapedEval mid_eval = eval_(split_pos);
      const SplineKnot k_mid{split_pos, mid_eval.T, mid_eval.dT};

      const auto insert_pos =
          result.begin() + numeric_cast<std::ptrdiff_t>(i) + 1;
      result.insert(insert_pos, k_mid);

      changed = true;
      ++i;  // Skip the newly created interval this pass
    }
  }

  return result;
}

auto AdaptiveSubdivider::subdivide_depth_first(
    std::vector<real_t> required_knots) -> std::vector<SplineKnot> {
  assert(!required_knots.empty());
  assert(std::is_sorted(required_knots.begin(), required_knots.end()));

  // Initialize with required knots.
  std::vector<SplineKnot> result;
  result.reserve(numeric_cast<std::size_t>(config_.max_segments));

  for (const real_t v : required_knots) {
    const ShapedEval e = eval_(v);
    result.push_back({v, e.T, e.dT});
  }

  // Subdivide each interval.
  size_t i = 0;
  while (i + 1 < result.size() &&
         numeric_cast<int>(result.size()) < config_.max_segments) {
    const SplineKnot& k0 = result[i];
    const SplineKnot& k1 = result[i + 1];

    const size_t size_before = result.size();
    subdivide_interval(k0.v, k1.v, k0, k1, 0, result);

    if (result.size() == size_before) {
      ++i;  // No splits, move to next interval.
    }
    // If splits occurred, recheck from same position.
  }

  return result;
}

auto AdaptiveSubdivider::subdivide_best_first(
    std::vector<real_t> required_knots) -> std::vector<SplineKnot> {
  struct KnotNode {
    SplineKnot knot;
    real_t error;      // Error of segment [this, next)
    real_t split_pos;  // Where to split this segment
  };

  std::list<KnotNode> knots;

  // Initialize from required positions
  for (real_t v : required_knots) {
    auto [T, dT] = eval_(v);
    knots.push_back({{v, T, dT}, 0, 0});
  }

  // Measure initial errors
  for (auto it = knots.begin(); it != knots.end(); ++it) {
    auto next = std::next(it);
    if (next == knots.end()) continue;

    auto [error, split_pos] =
        measure_error(it->knot.v, next->knot.v, it->knot, next->knot);
    it->error = error;
    it->split_pos = split_pos;
  }

  // Priority queue: (iterator, cached_error)
  struct PQEntry {
    std::list<KnotNode>::iterator it;
    real_t error;
    bool operator<(const PQEntry& o) const { return error < o.error; }
  };

  std::priority_queue<PQEntry> pq;

  for (auto it = knots.begin(); std::next(it) != knots.end(); ++it) {
    real_t width = std::next(it)->knot.v - it->knot.v;
    if (it->error > config_.tolerance || width > config_.max_width) {
      pq.push({it, it->error});
    }
  }

  // Split highest-error segments
  while (!pq.empty() &&
         numeric_cast<int>(knots.size()) < config_.max_segments + 1) {
    auto [it, cached_error] = pq.top();
    pq.pop();

    auto next = std::next(it);
    real_t split_v = it->split_pos;

    // Check min width
    if (split_v - it->knot.v < config_.min_width) continue;
    if (next->knot.v - split_v < config_.min_width) continue;

    // Insert new knot
    auto [T, dT] = eval_(split_v);
    auto new_it = knots.insert(next, {{split_v, T, dT}, 0, 0});

    // Recompute left segment [it, new_it)
    {
      auto [err, pos] =
          measure_error(it->knot.v, new_it->knot.v, it->knot, new_it->knot);
      it->error = err;
      it->split_pos = pos;
      real_t w = new_it->knot.v - it->knot.v;
      if (err > config_.tolerance || w > config_.max_width) {
        pq.push({it, err});
      }
    }

    // Compute right segment [new_it, next)
    {
      auto [err, pos] =
          measure_error(new_it->knot.v, next->knot.v, new_it->knot, next->knot);
      new_it->error = err;
      new_it->split_pos = pos;
      real_t w = next->knot.v - new_it->knot.v;
      if (err > config_.tolerance || w > config_.max_width) {
        pq.push({new_it, err});
      }
    }
  }

  // Extract in order (list is already sorted spatially)
  std::vector<SplineKnot> result;
  result.reserve(knots.size());
  for (const auto& node : knots) {
    result.push_back(node.knot);
  }

  return result;
}

// ============================================================================
// Hermite to Cubic Conversion
// ============================================================================

auto hermite_to_cubic(const SplineKnot& k0, const SplineKnot& k1)
    -> CubicCoeffs {
  // Hermite to standard form:
  //   a = 2(y0 - y1) + (m0 + m1)×w
  //   b = 3(y1 - y0) - (2m0 + m1)×w
  //   c = m0×w
  //   d = y0
  const real_t w = k1.v - k0.v;
  const real_t y0 = k0.T;
  const real_t y1 = k1.T;
  const real_t m0 = k0.dT;
  const real_t m1 = k1.dT;

  const real_t m0w = m0 * w;
  const real_t m1w = m1 * w;

  return {
      .a = 2 * y0 - 2 * y1 + m0w + m1w,
      .b = -3 * y0 + 3 * y1 - 2 * m0w - m1w,
      .c = m0w,
      .d = y0,
  };
}

// ============================================================================
// Fixed-Point Conversion
// ============================================================================

auto knot_to_fixed(real_t v) -> u32 {
  // Q8.24
  constexpr real_t scale =
      static_cast<real_t>(1UL << SHAPED_SPLINE_KNOT_FRAC_BITS);
  const real_t scaled = v * scale;

  if (scaled < 0) return 0;
  if (scaled >= static_cast<real_t>(UINT32_MAX)) return UINT32_MAX;

  return static_cast<u32>(scaled + 0.5L);
}

// ============================================================================
// k-ary Index Construction
// ============================================================================

void build_kary_index(shaped_spline& spline) {
  const int n = spline.num_segments;
  const u32* knots = spline.knots;

  // Level 0: 8 separators dividing into 9 regions.
  for (int i = 0; i < SHAPED_SPLINE_KARY_KEYS; ++i) {
    const int seg = (i + 1) * n / SHAPED_SPLINE_KARY_FANOUT;
    spline.kary_l0[i] = knots[std::min(seg, n)];
  }

  // Level 1: each L0 region subdivided into 9 sub-regions.
  for (int r0 = 0; r0 < SHAPED_SPLINE_KARY_L1_REGIONS; ++r0) {
    const int seg_start = r0 * n / SHAPED_SPLINE_KARY_FANOUT;
    const int seg_end = (r0 + 1) * n / SHAPED_SPLINE_KARY_FANOUT;
    const int region_size = seg_end - seg_start;

    for (int i = 0; i < SHAPED_SPLINE_KARY_KEYS; ++i) {
      int seg = seg_start + (i + 1) * region_size / SHAPED_SPLINE_KARY_FANOUT;
      seg = std::min(seg, n);
      spline.kary_l1[r0][i] = knots[seg];
    }
  }

  // Bucket base indices.
  for (int r0 = 0; r0 < SHAPED_SPLINE_KARY_L1_REGIONS; ++r0) {
    const int seg_start = r0 * n / SHAPED_SPLINE_KARY_FANOUT;
    const int seg_end = (r0 + 1) * n / SHAPED_SPLINE_KARY_FANOUT;
    const int region_size = seg_end - seg_start;

    for (int r1 = 0; r1 < SHAPED_SPLINE_KARY_FANOUT; ++r1) {
      const int bucket = r0 * SHAPED_SPLINE_KARY_FANOUT + r1;
      int seg = seg_start + r1 * region_size / SHAPED_SPLINE_KARY_FANOUT;
      seg = std::min(seg, n - 1);
      spline.kary_base[bucket] = numeric_cast<u8>(seg);
    }
  }
}

}  // namespace curves
