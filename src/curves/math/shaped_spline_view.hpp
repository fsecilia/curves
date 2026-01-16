// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Floating-point view of a shaped spline for UI display.

  This class wraps the fixed-point shaped_spline struct and provides
  floating-point evaluation for rendering curves in the UI. It converts
  the fixed-point coefficients back to floating point and evaluates
  T(v), T'(v), T''(v), then derives S(v), G(v), S'(v), G'(v) for display.

  Since input shaping and sensitivity offset are baked into the spline,
  this view is simpler than the old CurveView which had to compose
  shaping at evaluation time.

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

extern "C" {
#include <curves/driver/shaped_spline.h>
}

#include <curves/lib.hpp>
#include <curves/math/curves/spline/segment/view.hpp>
#include <curves/numeric_cast.hpp>
#include <cmath>
#include <vector>

namespace curves {

// ============================================================================
// Result Types
// ============================================================================

/// Result of evaluating T and its derivatives.
struct ShapedSplineResult {
  real_t T;    ///< Transfer function T(v).
  real_t dT;   ///< First derivative T'(v) = G(v).
  real_t d2T;  ///< Second derivative T''(v) = G'(v).
};

/// Result of evaluating all display curves.
struct ShapedCurveResult {
  real_t S;   ///< Sensitivity S(v) = T(v) / v.
  real_t dS;  ///< Sensitivity derivative S'(v).
  real_t G;   ///< Gain G(v) = T'(v).
  real_t dG;  ///< Gain derivative G'(v) = T''(v).
};

// ============================================================================
// Floating-Point Segment Data
// ============================================================================

/// Cached floating-point conversion of a segment.
struct ShapedSegmentFloat {
  real_t a, b, c, d;
  real_t inv_width;
  real_t knot;
  real_t width;
};

// ============================================================================
// ShapedSplineView
// ============================================================================

/*!
  Non-owning view of a shaped_spline with floating-point evaluation.

  Converts fixed-point data to floating point on construction and caches
  the result. Evaluation is pure floating-point arithmetic with no
  conversion in the hot path.

  The spline pointer must remain valid for the lifetime of this view.
*/
class ShapedSplineView {
 public:
  /// Construct a view of an existing shaped spline.
  explicit ShapedSplineView(const shaped_spline* spline);

  /// Default constructor creates an empty/invalid view.
  ShapedSplineView() = default;

  /// Returns true if this view points to valid spline data.
  [[nodiscard]] auto valid() const noexcept -> bool {
    return spline_ != nullptr && num_segments_ > 0;
  }

  /// Maximum velocity in the domain.
  [[nodiscard]] auto v_max() const noexcept -> real_t { return v_max_; }

  /// Number of segments in the spline.
  [[nodiscard]] auto num_segments() const noexcept -> int {
    return num_segments_;
  }

  /*!
    Evaluate T(v), T'(v), T''(v) at the given velocity.

    \param v Input velocity (clamped to [0, v_max]).
    \return Transfer function value and derivatives.
  */
  [[nodiscard]] auto operator()(real_t v) const -> ShapedSplineResult;

  /*!
    Evaluate just T(v) without derivatives.

    \param v Input velocity.
    \return T(v).
  */
  [[nodiscard]] auto eval(real_t v) const -> real_t;

  /*!
    Evaluate all display curves at the given velocity.

    Computes S, G, and their derivatives with proper limit handling at v = 0.

    \param v Input velocity.
    \return Sensitivity, gain, and their derivatives.
  */
  [[nodiscard]] auto curves_at(real_t v) const -> ShapedCurveResult;

  /*!
    Get the segment index containing velocity v.

    \param v Input velocity.
    \return Segment index in [0, num_segments).
  */
  [[nodiscard]] auto segment_at(real_t v) const -> int;

  /*!
    Get the knot position for a given index.

    \param idx Knot index in [0, num_segments].
    \return Knot position in velocity space.
  */
  [[nodiscard]] auto knot(int idx) const -> real_t;

 private:
  const shaped_spline* spline_ = nullptr;
  std::vector<ShapedSegmentFloat> segments_;
  std::vector<real_t> knots_;
  int num_segments_ = 0;
  real_t v_max_ = 0;

  static constexpr real_t kEpsilon = 1e-10L;

  [[nodiscard]] static auto knot_to_float(u32 fixed) -> real_t;
  [[nodiscard]] static auto coeff_to_float(s64 fixed, u8 shift) -> real_t;
  [[nodiscard]] static auto inv_width_to_float(u64 fixed, u8 shift) -> real_t;

  [[nodiscard]] auto find_segment(real_t v) const -> int;
  [[nodiscard]] auto eval_cubic(int seg_idx, real_t t) const
      -> ShapedSplineResult;
};

// ============================================================================
// Implementation
// ============================================================================

inline ShapedSplineView::ShapedSplineView(const shaped_spline* spline)
    : spline_{spline} {
  if (!spline) return;

  num_segments_ = spline->num_segments;
  if (num_segments_ <= 0) return;

  v_max_ = knot_to_float(spline->v_max);

  // Convert knots.
  knots_.reserve(numeric_cast<std::size_t>(num_segments_ + 1));
  for (int i = 0; i <= num_segments_; ++i) {
    knots_.push_back(knot_to_float(spline->knots[i]));
  }

  // Convert segments.
  segments_.reserve(numeric_cast<std::size_t>(num_segments_));
  for (auto i = 0; i < num_segments_; ++i) {
    const auto& packed_segment = spline->packed_segments[i];
    const auto& normalized_segment = segment::unpack(packed_segment);

    const real_t knot_v = knots_[numeric_cast<std::size_t>(i)];
    const real_t next_knot = knots_[numeric_cast<std::size_t>(i + 1)];
    const real_t width = next_knot - knot_v;

    segments_.push_back({
        .a = coeff_to_float(normalized_segment.poly.coeffs[0],
                            normalized_segment.poly.shifts[0]),
        .b = coeff_to_float(normalized_segment.poly.coeffs[1],
                            normalized_segment.poly.shifts[1]),
        .c = coeff_to_float(normalized_segment.poly.coeffs[2],
                            normalized_segment.poly.shifts[2]),
        .d = coeff_to_float(normalized_segment.poly.coeffs[3],
                            normalized_segment.poly.shifts[3]),
        .inv_width = inv_width_to_float(normalized_segment.inv_width.value,
                                        normalized_segment.inv_width.shift),
        .knot = knot_v,
        .width = width,
    });
  }
}

inline auto ShapedSplineView::knot_to_float(u32 fixed) -> real_t {
  constexpr real_t scale = 1.0L / (1ULL << SHAPED_SPLINE_KNOT_FRAC_BITS);
  return static_cast<real_t>(fixed) * scale;
}

inline auto ShapedSplineView::coeff_to_float(s64 fixed, u8 shift) -> real_t {
  return to_real(fixed, shift);
}

inline auto ShapedSplineView::inv_width_to_float(u64 fixed, u8 shift)
    -> real_t {
  return to_real<u64>(fixed, shift);
}

inline auto ShapedSplineView::find_segment(real_t v) const -> int {
  // Binary search for segment containing v.
  if (v <= knots_[0]) return 0;
  if (v >= knots_[numeric_cast<std::size_t>(num_segments_)]) {
    return num_segments_ - 1;
  }

  auto lo = 0;
  auto hi = num_segments_;

  while (lo < hi) {
    const auto mid = lo + (hi - lo) / 2;
    if (knots_[numeric_cast<std::size_t>(mid + 1)] <= v) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }

  return lo;
}

inline auto ShapedSplineView::eval_cubic(int seg_idx, real_t t) const
    -> ShapedSplineResult {
  const auto& seg = segments_[numeric_cast<std::size_t>(seg_idx)];

  // Horner's method for T.
  const real_t T = ((seg.a * t + seg.b) * t + seg.c) * t + seg.d;

  // First derivative: dT/dt = 3a*t^2 + 2b*t + c.
  // Chain rule: dT/dv = dT/dt * inv_width.
  const real_t dT_dt = (3 * seg.a * t + 2 * seg.b) * t + seg.c;
  const real_t dT = dT_dt * seg.inv_width;

  // Second derivative: d^2T/dt^2 = 6a*t + 2b.
  // Chain rule: d^2T/dv^2 = d^2T/dt^2 * inv_width^2.
  const real_t d2T_dt2 = 6 * seg.a * t + 2 * seg.b;
  const real_t d2T = d2T_dt2 * seg.inv_width * seg.inv_width;

  return {T, dT, d2T};
}

inline auto ShapedSplineView::operator()(real_t v) const -> ShapedSplineResult {
  if (!valid()) return {0, 0, 0};

  if (v < 0) v = 0;
  if (v > v_max_) v = v_max_;

  const int seg_idx = find_segment(v);
  const auto& seg = segments_[numeric_cast<std::size_t>(seg_idx)];

  real_t t = (seg.width > kEpsilon) ? (v - seg.knot) * seg.inv_width : 0;
  if (t < 0) t = 0;
  if (t > 1) t = 1;

  return eval_cubic(seg_idx, t);
}

inline auto ShapedSplineView::eval(real_t v) const -> real_t {
  if (!valid()) return 0;

  if (v < 0) v = 0;
  if (v > v_max_) v = v_max_;

  const int seg_idx = find_segment(v);
  const auto& seg = segments_[numeric_cast<std::size_t>(seg_idx)];

  real_t t = (seg.width > kEpsilon) ? (v - seg.knot) * seg.inv_width : 0;
  if (t < 0) t = 0;
  if (t > 1) t = 1;

  return ((seg.a * t + seg.b) * t + seg.c) * t + seg.d;
}

inline auto ShapedSplineView::curves_at(real_t v) const -> ShapedCurveResult {
  if (!valid()) return {0, 0, 0, 0};

  const auto [T, dT, d2T] = (*this)(v);

  const real_t G = dT;
  const real_t dG = d2T;

  // S(v) = T(v) / v with L'Hopital at v = 0.
  // S(0) = T'(0), S'(0) = T''(0) / 2.
  if (v < kEpsilon) {
    return {G, d2T / 2, G, dG};
  }

  const real_t S = T / v;
  const real_t dS = (G - S) / v;

  return {S, dS, G, dG};
}

inline auto ShapedSplineView::segment_at(real_t v) const -> int {
  if (!valid()) return 0;
  return find_segment(v);
}

inline auto ShapedSplineView::knot(int idx) const -> real_t {
  if (!valid() || idx < 0 || idx > num_segments_) return 0;
  return knots_[numeric_cast<std::size_t>(idx)];
}

}  // namespace curves
