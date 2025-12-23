// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#include "math.h"
#include "spline.h"

struct segment_params {
	s64 index;
	int width_log2;
};

/*
 * Subnormal Zone: Linear mapping.
 * All segments have constant, minimum width.
 * Index is x divided by that width.
 */
static inline struct segment_params subnormal_segment(s64 x)
{
	return (struct segment_params){
		.index = x >> SPLINE_MIN_SEGMENT_WIDTH_LOG2,
		.width_log2 = SPLINE_MIN_SEGMENT_WIDTH_LOG2
	};
}

/*
 * Geometric Octave: Logarithmic mapping.
 * Segment width doubles every octave.
 * Index = (start of octave) + (x_normalized - segments_per_octave).
 *
 * The offset is calculated by normalizing x to the current octave's
 * segment width, then masking out the leading implicit 1.
 */
static inline struct segment_params octave_segment(s64 x, int x_log2)
{
	int octave = x_log2 - SPLINE_DOMAIN_MIN_SHIFT;

	// Base index starts after the linear subnormal zone plus all previous
	// geometric octaves.
	s64 first_segment = ((s64)octave << SPLINE_SEGMENTS_PER_OCTAVE_LOG2) +
			    SPLINE_SEGMENTS_PER_OCTAVE;

	// Width scales with octave index.
	int width_log2 = SPLINE_MIN_SEGMENT_WIDTH_LOG2 + octave;

	// Normalize x to octave width to find offset.
	int segment_within_octave =
		(x >> width_log2) - SPLINE_SEGMENTS_PER_OCTAVE;

	return (struct segment_params){ .index = first_segment +
						 segment_within_octave,
					.width_log2 = width_log2 };
}

/*
 * Calculates t: The position of x within the segment, normalized to [0, 1).
 * t = (x % width) / width
 */
static inline s64 calc_t(s64 x, int width_log2)
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
static inline void locate_segment(s64 x, s64 *segment_index, s64 *t)
{
	struct segment_params params;
	int x_log2;

	if (WARN_ON_ONCE(!segment_index || !t))
		return;

	if (unlikely(x < 0)) {
		*segment_index = 0;
		*t = 0;
		return;
	}

	x_log2 = curves_log2_u64((u64)x);

	if (x_log2 < SPLINE_DOMAIN_MIN_SHIFT)
		params = subnormal_segment(x);
	else
		params = octave_segment(x, x_log2);

	*segment_index = params.index;
	*t = calc_t(x, params.width_log2);
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
		result = (s64)(((s128)result * t + SPLINE_FRAC_HALF) >>
			       SPLINE_FRAC_BITS);
		result += coeff[i];
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
	s64 t = calc_t(offset, spline->runout_width_log2);

	// Evaluate segment parametrically.
	return eval_segment(&spline->runout_segment, t);
}

s64 curves_spline_eval(const struct curves_spline *spline, s64 v)
{
	// Validate parameters.
	if (unlikely(v < 0))
		v = 0;

	// Transform from v in physical space to x in reference space.
	//
	// We scale the input velocity so that specific features (like cusps)
	// align with the fixed knot locations in our reference domain. Here,
	// we apply the transform and round.
	s64 x = (s64)(((s128)v * spline->v_to_x + SPLINE_FRAC_HALF) >>
		      SPLINE_FRAC_BITS);

	// Handle values beyond end of geometric progression.
	if (x >= spline->x_geometric_limit) {
		if (x >= spline->x_runout_limit)
			return extrapolate_linear(spline, x);

		return eval_runout(spline, x);
	}

	// Extract segment index and parameter t from x.
	s64 segment_index;
	s64 t;
	locate_segment(x, &segment_index, &t);

	// Evaluate segment in parametric space.
	return eval_segment(&spline->segments[segment_index], t);
}
