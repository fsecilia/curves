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

namespace curves {
namespace spline {

auto locate_knot(int knot) noexcept -> s64;
auto locate_segment(s64 x, s64* segment_index, s64* t) noexcept -> void;
auto eval(const struct curves_spline* spline, s64 x) noexcept -> s64;

// Knot to form cubic hermite splines, {x, y, dy/dx}.
struct Knot {
  real_t x;
  real_t y;
  real_t m;
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
  auto operator()(const Knot& k0, const Knot& k1) const noexcept
      -> curves_spline_segment {
    const auto dx = k1.x - k0.x;
    const auto dy = k1.y - k0.y;
    const auto m0 = k0.m * dx;
    const auto m1 = k1.m * dx;

    return {.coeffs = {Fixed{-2 * dy + m0 + m1}.value,
                       Fixed{3 * dy - 2 * m0 - m1}.value, Fixed{m0}.value,
                       Fixed{k0.y}.value}};
  }
};

// Encapsulates how knots are located.
struct KnotLocator {
  auto operator()(int i) const noexcept -> real_t {
    // Call out to shared c implementation.
    return Fixed::literal(locate_knot(i)).to_real();
  }
};

// Samples a curve to create a knot.
template <typename KnotLocator = KnotLocator>
class KnotSampler {
 public:
  explicit KnotSampler(KnotLocator locator) noexcept
      : locator_{std::move(locator)} {}

  KnotSampler() = default;

  auto operator()(const auto& curve, real_t sensitivity, int_t knot) const
      -> Knot {
    const auto x = locator_(knot);
    const auto [f, df_dx] = curve(x);
    return {x, f * sensitivity, df_dx * sensitivity};
  }

 private:
  KnotLocator locator_;
};

/*
  Builds a spline by sampling a curve for knots, then building segments
  between the knots.
*/
template <int_t num_segments, typename KnotSampler = KnotSampler<>,
          typename SegmentConverter = SegmentConverter>
class SplineBuilder {
 public:
  SplineBuilder(KnotSampler knot_sampler,
                SegmentConverter segment_converter) noexcept
      : knot_sampler_{std::move(knot_sampler)},
        segment_converter_{std::move(segment_converter)} {}

  SplineBuilder() = default;

  auto operator()(const auto& curve, real_t sensitivity) const noexcept
      -> curves_spline {
    curves_spline result;

    auto k0 = knot_sampler_(curve, sensitivity, 0);
    for (auto segment = 0; segment < num_segments - 1; ++segment) {
      const auto k1 = knot_sampler_(curve, sensitivity, segment + 1);
      result.segments[segment] = segment_converter_(k0, k1);
      k0 = k1;
    }

    construct_runout_segment(curve, sensitivity,
                             result.segments[num_segments - 2],
                             result.segments[num_segments - 1]);

    return result;
  }

 private:
  KnotSampler knot_sampler_;
  SegmentConverter segment_converter_;

  /*
    Bleeds off curvature before straightening out so final tangent can be
    linearly extended without a kink in gain when evaluating beyond the final
    segment.
  */
  auto construct_runout_segment(const auto& curve, real_t sensitivity,
                                const curves_spline_segment& prev,
                                curves_spline_segment& next) const noexcept
      -> void {
    const auto k_prev_end = knot_sampler_(curve, sensitivity, num_segments - 1);
    const auto k_prev_start =
        knot_sampler_(curve, sensitivity, num_segments - 2);
    const double w_prev = k_prev_end.x - k_prev_start.x;

    // Fetch previous segment coefficients (raw s64 fixed-point values)
    const auto prev_a = Fixed::literal(prev.coeffs[0]).to_real();
    const auto prev_b = Fixed::literal(prev.coeffs[1]).to_real();
    const auto prev_c = Fixed::literal(prev.coeffs[2]).to_real();
    const auto prev_d = Fixed::literal(prev.coeffs[3]).to_real();

    double y_start = prev_a + prev_b + prev_c + prev_d;
    double m_start_norm =
        3.0 * prev_a + 2.0 * prev_b + prev_c;           // Normalized slope
    double k_start_norm = 6.0 * prev_a + 2.0 * prev_b;  // Normalized curvature

    // 2. Un-normalize derivatives to real units
    double m_real = m_start_norm / w_prev;
    double k_real = k_start_norm / (w_prev * w_prev);

    // 3. Define new segment width (start of new octave = 2x width)
    double w_new = w_prev * 2.0;

    // 4. Calculate d (Position)
    double next_d = y_start;

    // 5. Calculate c (Velocity match)
    // Renormalize real slope to new width
    double next_c = m_real * w_new;

    // 6. Calculate b (Curvature match)
    // We want the curvature at t=0 to match k_real.
    // y''(0) = 2b / w_new^2 = k_real
    double next_b = (k_real * w_new * w_new) / 2.0;

    // 7. Calculate a (Zero curvature target)
    // We want y''(1) = 0.
    // 6a + 2b = 0  ->  a = -b / 3
    double next_a = -next_b / 3.0;

    next.coeffs[0] = Fixed{next_a}.value;
    next.coeffs[1] = Fixed{next_b}.value;
    next.coeffs[2] = Fixed{next_c}.value;
    next.coeffs[3] = Fixed{next_d}.value;
  }
};

inline auto create_spline(const auto& curve, real_t sensitivity) noexcept
    -> curves_spline {
  return SplineBuilder<SPLINE_NUM_SEGMENTS, KnotSampler<>, SegmentConverter>{}(
      curve, sensitivity);
}

}  // namespace spline
}  // namespace curves
