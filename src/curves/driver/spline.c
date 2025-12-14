// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#include "math.h"
#include "spline.h"

// Calculates sample location for a given knot index.
s64 curves_spline_locate_knot(int knot)
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

// Finds segment index and interpolation for input x.
//
// Given input x, determines:
//   - segment_index: which spline segment contains x
//   - t: position within segment, normalized to [0, 1) in fixed-point
void curves_spline_locate_segment(s64 x, s64 *segment_index, s64 *t)
{
	int x_log2, octave_segment_width_log2;
	u64 mask, remainder;

	if (WARN_ON_ONCE(!segment_index || !t))
		return;

	if (WARN_ON_ONCE(x < 0)) {
		*segment_index = 0;
		*t = 0;
		return;
	}

	// Determine whether x is in the subnormal zone or an octave.
	x_log2 = curves_log2_u64((u64)x);
	if (x_log2 < SPLINE_DOMAIN_MIN_SHIFT) {
		// Subnormal zone.
		//
		// This region is linear. All values below use minimum-width
		// segments.
		octave_segment_width_log2 = SPLINE_MIN_SEGMENT_WIDTH_LOG2;
		*segment_index = x >> octave_segment_width_log2;
	} else {
		// Geometric octave.
		//
		// Octave 0 covers [DOMAIN_MIN, 2*DOMAIN_MIN) with min width.
		// Each subsequent octave doubles in width.
		int segment_within_octave;

		int octave = x_log2 - SPLINE_DOMAIN_MIN_SHIFT;

		s64 first_segment_in_octave =
			((s64)octave << SPLINE_SEGMENTS_PER_OCTAVE_LOG2) +
			SPLINE_SEGMENTS_PER_OCTAVE;
		octave_segment_width_log2 =
			SPLINE_MIN_SEGMENT_WIDTH_LOG2 + octave;
		segment_within_octave = (x >> octave_segment_width_log2) -
					SPLINE_SEGMENTS_PER_OCTAVE;
		*segment_index =
			first_segment_in_octave + segment_within_octave;
	}

	// Interpolation parameter, t.
	//
	// t is the fractional position within the segment, scaled to
	// SPLINE_FRAC_BITS precision. Extract the bits below the segment
	// boundary
	mask = (1ULL << octave_segment_width_log2) - 1;
	remainder = x & mask;
	if (octave_segment_width_log2 < SPLINE_FRAC_BITS)
		*t = remainder
		     << (SPLINE_FRAC_BITS - octave_segment_width_log2);
	else
		*t = remainder >>
		     (octave_segment_width_log2 - SPLINE_FRAC_BITS);
}

// Calculates linear extension for x >= x_max.
static s64 curves_spline_extend_linear(const struct curves_spline *spline,
				       s64 x)
{
	s64 x_max = curves_spline_locate_knot(SPLINE_NUM_SEGMENTS);
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

static s64
curves_spline_eval_segment(const struct curves_spline_segment *segment, s64 t)
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

s64 curves_spline_eval(const struct curves_spline *spline, s64 x)
{
	// Validate parameters.
	if (unlikely(x < 0))
		x = 0;
	if (unlikely(x >= curves_spline_locate_knot(SPLINE_NUM_SEGMENTS))) {
		return curves_spline_extend_linear(spline, x);
	}

	s64 segment_index;
	s64 t;
	curves_spline_locate_segment(x, &segment_index, &t);

	return curves_spline_eval_segment(&spline->segments[segment_index], t);
}
