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

// Calculates sample location for a given knot index.
static s64 locate_knot(int knot)
{
	int octave, octave_segment_width_log2, segment_within_octave;
	s64 segment;

	// Origin must be below octave 0.
	BUILD_BUG_ON(SPLINE_DOMAIN_MIN_SHIFT < SPLINE_SEGMENTS_PER_OCTAVE_LOG2);

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
 * Scaled to SPLINE_FRAC_BITS fixed-point.
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

// Calculates linear extension for x >= x_max.
static s64 extend_linear(const struct curves_spline *spline, s64 x)
{
	s64 x_max = locate_knot(SPLINE_NUM_SEGMENTS);
	int last_idx = SPLINE_NUM_SEGMENTS - 1;

	// Calc shift (width) of that last segment to scale the slope.
	// We can infer it from the index.
	int m_bits = SPLINE_SEGMENTS_PER_OCTAVE_LOG2;
	int b_bias = SPLINE_DOMAIN_MIN_SHIFT;
	int block = last_idx >> m_bits;
	int last_shift = (block == 0) ? (b_bias - m_bits) :
					(b_bias + block - 1 - m_bits);

	// Extract coefficients
	const s64 *coeff = spline->segments[last_idx].coeffs;
	s64 a = *coeff++;
	s64 b = *coeff++;
	s64 c = *coeff++;
	s64 d = *coeff++;

	// Calculate Slope
	// dy/dt = 3a + 2b + c
	// m = dy/dt / width = dy/dt / 2^last_shift
	s128 dy_dt = (s128)3 * a + (s128)2 * b + c;

	int scale = SPLINE_FRAC_BITS - last_shift;
	s64 slope;

	if (scale >= 0)
		slope = (s64)(dy_dt << scale);
	else
		slope = (s64)(dy_dt >> (-scale));

	// result = slope * (x - x_max) + y_max
	s64 y_max = a + b + c + d;
	return (s64)(((s128)slope * (x - x_max)) >> SPLINE_FRAC_BITS) + y_max;
}

static s64 eval_segment(const struct curves_spline_segment *segment, s64 t)
{
	// Horner's method: ((a*t + b)*t + c)*t + d
	//
	// Each multiplication truncates. Adding half before the shift rounds
	// to nearest, reducing worst-case relative error by about 3x.
	const s64 *coeff = segment->coeffs;
	s64 result = *coeff++;
	for (int i = 1; i < SPLINE_NUM_COEFFS; ++i) {
		result = (s64)(((s128)result * t + SPLINE_FRAC_HALF) >>
			       SPLINE_FRAC_BITS);
		result += *coeff++;
	}

	return result;
}

s64 curves_spline_eval(const struct curves_spline *spline, s64 v)
{
	// Validate parameters.
	if (unlikely(v < 0))
		v = 0;

	// Coordinate Transformation: Physical Space (v) -> Reference Space (x)
	//
	// We scale the input velocity so that specific features (like cusps)
	// align with the fixed knot locations in our reference domain. Here,
	// we apply the transform and round.
	s64 x = (s64)(((s128)v * spline->v_to_x + SPLINE_FRAC_HALF) >>
		      SPLINE_FRAC_BITS);

	// Handle inputs beyond end of mapped domain.
	if (x >= locate_knot(SPLINE_NUM_SEGMENTS))
		return extend_linear(spline, x);

	// Extract segment index and parameter t from x.
	s64 segment_index;
	s64 t;
	locate_segment(x, &segment_index, &t);

	// Evaluate segment in parametric space.
	return eval_segment(&spline->segments[segment_index], t);
}
