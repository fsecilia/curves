// SPDX-License-Identifier: MIT
/*!
  \file
  \brief Floating-point spline sampler for the ui.

  The spline is implemented in fixed-point c. This class wraps it and returns
  information necessary to synthesize sensitivity, gain, and their derivatives
  from the transfer function it approximates.

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/lib.hpp>
#include <curves/math/fixed.hpp>
#include <curves/math/spline.hpp>

namespace curves {

struct SplineSample {
  // Cubic coefficients for T(t) = at^3 + bt^2 + ct + d
  real_t a, b, c, d;

  // Interpolation parameter t.
  // Usually [0, 1), but may be >= 0 in the linear extension.
  real_t t;

  // Inverse width (1.0 / width).
  // Used for Chain Rule: G = T' * inv_width
  real_t inv_width;

  // True only if we are in the very first segment [0, knot_1).
  // The first segment is noise divided by 0, so it needs special handling.
  bool is_start_segment;
};

template <typename KnotLocator = spline::KnotLocator>
class SplineSampler {
 public:
  explicit SplineSampler(const curves_spline* spline,
                         KnotLocator knot_locator = {})
      : m_spline(spline), knot_locator_{std::move(knot_locator)} {}

  auto sample(real_t v) const -> SplineSample {
    if (v < 0) v = 0;

    // 1. Transform Physical Space (v) -> Reference Space (x)
    // We use Fixed types to match the kernel's precision exactly.
    const auto v_to_x = Fixed::from_raw(m_spline->v_to_x);
    const auto x_fixed = Fixed{v} * v_to_x;

    // 2. Check Boundaries (Using the new struct limits)

    // Case A: Linear Extension (Beyond Runout)
    if (x_fixed.raw >= m_spline->x_runout_limit) {
      return sample_linear_extension(v);
    }

    // Case B: Runout Segment (Between Grid and Extension)
    if (x_fixed.raw >= m_spline->x_geometric_limit) {
      return convert_runout(x_fixed.raw, v_to_x);
    }

    // Case C: Geometric Grid
    // We reuse the C kernel's efficient bitwise locator.
    const auto spline_coords = spline::resolve_x(x_fixed.raw);

    return convert_geometric(spline_coords.segment_index, spline_coords.t,
                             v_to_x);
  }

 private:
  const curves_spline* m_spline;
  KnotLocator knot_locator_{};

  // Helper to convert fixed-point coeffs to float struct
  auto make_sample(const s64* coeffs, real_t t, real_t inv_width,
                   bool is_start) const -> SplineSample {
    return SplineSample{.a = Fixed::from_raw(coeffs[0]).to_real(),
                        .b = Fixed::from_raw(coeffs[1]).to_real(),
                        .c = Fixed::from_raw(coeffs[2]).to_real(),
                        .d = Fixed::from_raw(coeffs[3]).to_real(),
                        .t = t,
                        .inv_width = inv_width,
                        .is_start_segment = is_start};
  }

  // Handle standard geometric segments
  auto convert_geometric(s64 segment_idx, s64 t_fixed, Fixed v_to_x) const
      -> SplineSample {
    const auto& seg = m_spline->segments[segment_idx];

    // Calculate Physical Width (dv)
    // dv = dx / v_to_x_scalar  OR  dv = dx * (1/v_to_x_scalar)
    // But we have v_to_x. So dv = dx / v_to_x.

    // Get Width in Reference Space (dx) from Locator
    const s64 x_start = knot_locator_(segment_idx);
    const s64 x_end = knot_locator_(segment_idx + 1);
    const real_t dx = Fixed::from_raw(x_end - x_start).to_real();

    // Convert to Physical Width
    const real_t dv = dx / v_to_x.to_real();

    return make_sample(seg.coeffs, Fixed::from_raw(t_fixed).to_real(),
                       (dv > 0) ? 1.0 / dv : 0.0, segment_idx == 0);
  }

  // Handle the detached runout segment
  auto convert_runout(s64 x_current, Fixed v_to_x) const -> SplineSample {
    // Calculate t using the struct's optimization
    // t = (x - start) / width
    s64 offset = x_current - m_spline->x_geometric_limit;
    s64 t_fixed = spline::map_x_to_t(offset, m_spline->runout_width_log2);

    // Calculate Width
    // width = 1 << log2
    s64 width_fixed = 1ULL << m_spline->runout_width_log2;
    real_t dx = Fixed::from_raw(width_fixed).to_real();
    real_t dv = dx / v_to_x.to_real();

    return make_sample(m_spline->runout_segment.coeffs,
                       Fixed::from_raw(t_fixed).to_real(), 1.0 / dv, false);
  }

  // Handle linear extrapolation
  auto sample_linear_extension(real_t v) const -> SplineSample {
    // We construct a "Linear Segment" that starts at the end of the runout.
    // y = m(v - v_start) + y_start

    // Calculate Physical Slope (m_phys) from Runout Segment
    const auto& r_seg = m_spline->runout_segment;
    double ra = Fixed::from_raw(r_seg.coeffs[0]).to_real();
    double rb = Fixed::from_raw(r_seg.coeffs[1]).to_real();
    double rc = Fixed::from_raw(r_seg.coeffs[2]).to_real();
    double rd = Fixed::from_raw(r_seg.coeffs[3]).to_real();

    // Slope at t=1 in Parametric Space
    // P'(1) = 3a + 2b + c
    double m_param = 3.0 * ra + 2.0 * rb + rc;

    // Convert to Physical Slope: m_phys = m_param * inv_width_runout
    s64 r_width_fixed = 1ULL << m_spline->runout_width_log2;
    real_t r_dx = Fixed::from_raw(r_width_fixed).to_real();
    real_t r_dv = r_dx / Fixed::from_raw(m_spline->v_to_x).to_real();
    double m_phys = m_param * (1.0 / r_dv);

    // Calculate Y Start (Value at runout t=1)
    double y_start = ra + rb + rc + rd;

    // Calculate v_start (Physical start of extension)
    real_t x_runout_end = Fixed::from_raw(m_spline->x_runout_limit).to_real();
    real_t v_start = x_runout_end / Fixed::from_raw(m_spline->v_to_x).to_real();

    // Synthesize Sample
    // We set up a linear polynomial: Y(t) = c*t + d
    // Where t = (v - v_start) and width = 1.0.
    return SplineSample{.a = 0,
                        .b = 0,
                        .c = m_phys,       // Slope
                        .d = y_start,      // Intercept
                        .t = v - v_start,  // Distance from end of spline
                        .inv_width = 1.0,  // Unit width for 1:1 mapping
                        .is_start_segment = false};
  }
};

}  // namespace curves
