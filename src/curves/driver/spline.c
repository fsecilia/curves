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

/*
 * Geometric Octave: Logarithmic mapping.
 * Segment width doubles every octave.
 * index = (start of octave) + (x_uniform - segments_per_octave).
 *
 * In a geometric progression, the sum total width of all previous octaves is
 * the same width as the current octave. We can remap the domain logically into
 * 2 octave's worth of uniform segments at the current octave's segment width.
 * If we divide x by this segment width, we find its index in this uniform
 * sequence, placing it in the second octave. Subtract off the first logical
 * octave's worth of segments, and you have the index of the segment containing
 * x relative to the first segment in the current octave.
 *
 * remap this:
 *              octave 2        octave 3
 *      octave 1       |               |
 *             |       |               |
 *     ________[][][][][__][__][__][__][______][__x___][______][______]
 *     |   |
 *     |   octave 0
 *     0
 *
 * to this:
 *                              octave 3
 *                                     |
 *     [______][______][______][______][______][__x___][______][______]
 *     |
 *     0
 *
 * 4 octaves per segment, current segment width is 8, x = 43
 * x/8 = 5, x/8 - 4 = 1. x is in segment 1 of the current octave.
 *
 * The geometry of segments is progressive, but they are indexed linearly.
 * Relative to octave 0, the index of the first segment in an octave is just
 * `octave*segments_per_octave`. We also have to account for the subnormal zone
 * at the beginning of the segment array, before octave 0. There is one
 * octave's worth of segments there, so we add one octave's worth of segments
 * to the index relative to 0 to find the global index.
 *
 */
static inline struct curves_segment_desc calc_octave_segment_desc(s64 x,
								  int x_log2)
{
	int octave = x_log2 - SPLINE_DOMAIN_MIN_SHIFT;
	int segment_width_log2 = SPLINE_MIN_SEGMENT_WIDTH_LOG2 + octave;
	s64 first_octave_segment =
		((s64)octave << SPLINE_SEGMENTS_PER_OCTAVE_LOG2) +
		SPLINE_SEGMENTS_PER_OCTAVE;
	s64 x_uniform = x >> segment_width_log2;
	s64 segment_within_octave = x_uniform - SPLINE_SEGMENTS_PER_OCTAVE;

	return (struct curves_segment_desc){ .index = first_octave_segment +
						      segment_within_octave,
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
	if (width_log2 < SPLINE_FRAC_BITS)
		return remainder << (SPLINE_FRAC_BITS - width_log2);
	else
		return remainder >> (width_log2 - SPLINE_FRAC_BITS);
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

	if (x_log2 < SPLINE_DOMAIN_MIN_SHIFT)
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
	s64 scale_log2 = SPLINE_FRAC_BITS - spline->runout_width_log2;
	s64 slope;
	if (scale_log2 >= 0)
		slope = (s64)(dy_dt << scale_log2);
	else
		slope = (s64)(dy_dt >> -scale_log2);

	// result = slope * t + y_start
	return (s64)(((s128)slope * t + SPLINE_FRAC_HALF) >> SPLINE_FRAC_BITS) +
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
