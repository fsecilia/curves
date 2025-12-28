// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#include "math.h"
#include "spline.h"

/**
 * calc_subnormal_segment_desc - Compute segment descriptor for subnormal zone
 * @x: Input value in the subnormal zone
 *
 * Subnormal zone has a linear mapping. All segments have constant, minimum
 * width. Covers indices [0, SEGMENTS_PER_OCTAVE).
 *
 * index = x / segment_width
 *
 * Return: Segment descriptor with index and width_log2.
 */
static inline struct curves_segment_desc calc_subnormal_segment_desc(s64 x)
{
	return (struct curves_segment_desc){
		.index = x >> SPLINE_MIN_SEGMENT_WIDTH_LOG2,
		.width_log2 = SPLINE_MIN_SEGMENT_WIDTH_LOG2
	};
}

/**
 * calc_octave_segment_desc - Compute segment descriptor for geometric octave
 * @x: Input value in a geometric octave
 * @x_log2: Integer log2 of x
 *
 * Geometric octaves have a logarithmic mapping. Segment width doubles every
 * octave. Covers indices [SEGMENTS_PER_OCTAVE, ...).
 *
 * index = prior_octave_segments + x / segment_width
 *
 * Return: Segment descriptor with index and width_log2.
 */
static inline struct curves_segment_desc calc_octave_segment_desc(s64 x,
								  int x_log2)
{
	int octave = x_log2 - SPLINE_OCTAVE_ORIGIN_FIXED_SHIFT;
	int segment_width_log2 = SPLINE_MIN_SEGMENT_WIDTH_LOG2 + octave;

	/*
	 * The sum of all prior octaves' widths and the width of the subnormal
	 * zone is the same as the current octave's width. That puts
	 * x/segment_width in [SEGMENTS_PER_OCTAVE, 2*SEGMENTS_PER_OCTAVE),
	 * accounting for the subnormal zone offset naturally:
	 *   - octave 0: [SEGMENTS_PER_OCTAVE, 2*SEGMENTS_PER_OCTAVE)
	 *   - octave N: [(N+1)*SEGMENTS_PER_OCTAVE, (N+2)*SEGMENTS_PER_OCTAVE)
	 */
	s64 prior_octave_segments = (s64)octave
				    << SPLINE_SEGMENTS_PER_OCTAVE_LOG2;
	s64 x_in_segment_widths = x >> segment_width_log2;

	return (struct curves_segment_desc){ .index = prior_octave_segments +
						      x_in_segment_widths,
					     .width_log2 = segment_width_log2 };
}

/*
 * Calculates t: The position of x within the segment, normalized to [0, 1).
 * t = (x % width) / width
 */
static inline s64 map_x_to_t(s64 x, int width_log2)
{
	u64 mask = (1ULL << width_log2) - 1;
	u64 remainder = x & mask;

	// Shift to normalize remainder to SPLINE_FRAC_BITS
	if (width_log2 < CURVES_FIXED_SHIFT)
		return remainder << (CURVES_FIXED_SHIFT - width_log2);
	else
		return remainder >> (width_log2 - CURVES_FIXED_SHIFT);
}

// Finds segment index and interpolation for input x.
static inline struct curves_spline_coords resolve_x(s64 x)
{
	struct curves_segment_desc segment_geometry;
	struct curves_spline_coords result;
	int x_log2;

	if (unlikely(x < 0)) {
		result.segment_index = 0;
		result.t = 0;
		return result;
	}

	x_log2 = curves_log2_u64((u64)x);

	if (x_log2 < SPLINE_OCTAVE_ORIGIN_FIXED_SHIFT)
		segment_geometry = calc_subnormal_segment_desc(x);
	else
		segment_geometry = calc_octave_segment_desc(x, x_log2);

	result.segment_index = segment_geometry.index;
	result.t = map_x_to_t(x, segment_geometry.width_log2);
	return result;
}

/*
 * Linear Extension via Extrapolation
 *
 * Extends the spline tangentially beyond the runout segment.
 */
static s64 extrapolate_linear(const struct curves_spline *spline, s64 x)
{
	const s64 *c = spline->runout_segment.coeffs;

	// Find slope at t = 1: dy/dt = 3a + 2b + c
	s128 dy_dt = 3 * (s128)c[0] + 2 * (s128)c[1] + (s128)c[2];

	// Start (x, y) at t = 1: y = a + b + c + d
	s64 y_start = c[0] + c[1] + c[2] + c[3];
	s64 x_start = spline->x_runout_limit;
	s64 t = x - x_start;

	// Transform slope: dy/dx = (dy/dt)/segment_width
	s64 scale_log2 = CURVES_FIXED_SHIFT - spline->runout_width_log2;
	s64 slope;
	if (scale_log2 >= 0)
		slope = (s64)(dy_dt << scale_log2);
	else
		slope = (s64)(dy_dt >> -scale_log2);

	// result = slope * t + y_start
	return (s64)(((s128)slope * t + CURVES_FIXED_HALF) >>
		     CURVES_FIXED_SHIFT) +
	       y_start;
}

/*
 * Evaluates segment parametrically.
*/
static s64 eval_segment(const struct curves_spline_segment *segment, s64 t)
{
	// Horner's method, with rounding: ((a*t + b)*t + c)*t + d
	const s64 *coeff = segment->coeffs;
	s64 result = coeff[0];
	for (int i = 1; i < SPLINE_NUM_COEFFS; ++i) {
		result = curves_fixed_fma_round(result, t, coeff[i]);
	}

	return result;
}

/*
 * Runout Evaluation
 *
 * The runout segment does not follow the same geometric progression in width
 * as the segment array does. It is as wide as an octave itself to slowly bleed
 * off curvature at the final segment's final tangent. This way, when we extend
 * the curve beyond the runout segment by linear extrapolation, it is already
 * straight.
 */
static s64 eval_runout(const struct curves_spline *spline, s64 x)
{
	// Translate x local to segment origin.
	s64 offset = x - spline->x_geometric_limit;

	// Convert x in reference space to t in parametric space.
	s64 t = map_x_to_t(offset, spline->runout_width_log2);

	// Evaluate segment parametrically.
	return eval_segment(&spline->runout_segment, t);
}

// Transform from v in physical space to x in reference space.
//
// We scale the input velocity so that specific features (like cusps)
// align with the fixed knot locations in our reference domain. Here,
// we apply the transform and round.
static s64 map_v_to_x(const struct curves_spline *spline, s64 v)
{
	return curves_fixed_multiply_round(v, spline->v_to_x);
}

s64 curves_spline_eval(const struct curves_spline *spline, s64 v)
{
	struct curves_spline_coords spline_coords;
	s64 x;

	// Validate parameters.
	if (unlikely(v < 0))
		v = 0;

	x = map_v_to_x(spline, v);

	// Handle values beyond end of geometric progression.
	if (x >= spline->x_geometric_limit) {
		if (x >= spline->x_runout_limit)
			return extrapolate_linear(spline, x);

		return eval_runout(spline, x);
	}

	// Extract segment index and parameter t from x.
	spline_coords = resolve_x(x);

	// Evaluate segment in parametric space.
	return eval_segment(&spline->segments[spline_coords.segment_index],
			    spline_coords.t);
}
