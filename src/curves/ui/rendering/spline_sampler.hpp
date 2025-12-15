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
#include <curves/fixed.hpp>
#include <curves/spline.hpp>

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

class SplineSampler {
 public:
  explicit SplineSampler(const curves_spline* spline) : m_spline(spline) {}

  auto sample(real_t x_logical) const -> SplineSample {
    if (x_logical < 0) x_logical = 0;

    s64 segment;
    s64 t_fixed;
    const auto x_fixed = Fixed{x_logical};
    curves::spline::locate_segment(x_fixed.value, &segment, &t_fixed);

    if (segment >= SPLINE_NUM_SEGMENTS) return extend_linearly(x_logical);

    return convert(segment, t_fixed);
  }

 private:
  const curves_spline* m_spline;

  auto convert(s64 segment, s64 t_fixed) const -> SplineSample {
    const auto& seg = m_spline->segments[segment];

    // Calculate width in domain units.
    const s64 x_start = curves::spline::locate_knot(segment);
    const s64 x_end = curves::spline::locate_knot(segment + 1);
    const double width = curves::Fixed::literal(x_end - x_start).to_real();

    return SplineSample{.a = Fixed::literal(seg.coeffs[0]).to_real(),
                        .b = Fixed::literal(seg.coeffs[1]).to_real(),
                        .c = Fixed::literal(seg.coeffs[2]).to_real(),
                        .d = Fixed::literal(seg.coeffs[3]).to_real(),
                        .t = Fixed::literal(t_fixed).to_real(),
                        .inv_width = (width > 0.0 ? 1.0 / width : 0.0),
                        .is_start_segment = (segment == 0)};
  }

  auto extend_linearly(double x_logical) const -> SplineSample {
    // Build the extension based on the geometry of the valid segment.
    const int segment = SPLINE_NUM_SEGMENTS - 1;

    // Grab the base frame.
    auto frame = convert(segment, 0);

    // Calculate the values at the end of the last segment, at t = 1.
    // Slope P'(1) = 3a + 2b + c
    const auto last_slope_local = 3.0 * frame.a + 2.0 * frame.b + frame.c;
    // Value T(1) = a + b + c + d
    const auto last_value = frame.a + frame.b + frame.c + frame.d;

    // Synthesize a Linear Segment.
    const auto x_end_fixed = curves::spline::locate_knot(SPLINE_NUM_SEGMENTS);
    const auto x_end_logical = curves::Fixed::literal(x_end_fixed).to_real();
    const auto dx = x_logical - x_end_logical;

    frame.a = 0;
    frame.b = 0;
    frame.c = last_slope_local;  // acts as linear slope
    frame.d = last_value;        // acts as intercept

    // t = dx / width.
    frame.t = dx * frame.inv_width;

    // Extension is never the start segment
    frame.is_start_segment = false;

    return frame;
  }
};

}  // namespace curves
