// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Cubic Hermite spline interpolation to approximate sensitivity curves.
 *
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#ifndef _CURVES_SPLINE_H
#define _CURVES_SPLINE_H

#include "fixed.h"
#include "math.h"

// ----------------------------------------------------------------------------
// Tunable Parameters
// ----------------------------------------------------------------------------
// These are only slightly tunable, but the are not derived constants. Be wary
// of modifying them.
// ----------------------------------------------------------------------------

// Fractional bits of the fixed-point format.
#define SPLINE_FRAC_BITS 32

// Domain minimum.
//
// Smallest input the spline handles with full geometric resolution. Below
// this, segments have constant, minimum width.
//
// Increasing it makes the smallest segments of the grid wider, making the grid
// coarser, but reducing the total number of segments. Decreasing it makes them
// smaller, increasing the resolution of the grid, but increasing the number of
// segments. Every -1 adds a whole octave's worth of segments.
#define SPLINE_DOMAIN_MIN_LOG2 -8

// Domain maximum.
//
// Largest input covered by spline segments. Above this, output extrapolates
// linearly using the final segment's slope.
//
// 2^7 = 128 exceeds typical mouse velocity. 2^6 = 64 does not.
#define SPLINE_DOMAIN_MAX_LOG2 7

// Segments per octave.
//
// How finely each octave is subdivided.
//
// Empirically, given SPLINE_DOMAIN_MIN_LOG2 == -8,
// 3 = 8/octave: less accurate, fewer segments
// 4 = 16/octave: balances accuracy, number of segments
// 5 = 32/octave: more accurate, more segments
#define SPLINE_SEGMENTS_PER_OCTAVE_LOG2 4
#define SPLINE_SEGMENTS_PER_OCTAVE (1LL << SPLINE_SEGMENTS_PER_OCTAVE_LOG2)

// ----------------------------------------------------------------------------
// Derived Parameters
// ----------------------------------------------------------------------------

// Bit position of DOMAIN_MIN in fixed-point representation.
#define SPLINE_DOMAIN_MIN_SHIFT (SPLINE_FRAC_BITS + SPLINE_DOMAIN_MIN_LOG2)

// Width of smallest segments.
// Octave 0 (linear) and octave 1 share this width; doubling starts at octave 2.
#define SPLINE_MIN_SEGMENT_WIDTH_LOG2 \
	(SPLINE_DOMAIN_MIN_SHIFT - SPLINE_SEGMENTS_PER_OCTAVE_LOG2)

// Total octaves needed to span span min to max.
#define SPLINE_NUM_OCTAVES (SPLINE_DOMAIN_MAX_LOG2 - SPLINE_DOMAIN_MIN_LOG2)

// Total segments needed to cover all octaves.
#define SPLINE_NUM_SEGMENTS \
	(SPLINE_NUM_OCTAVES << SPLINE_SEGMENTS_PER_OCTAVE_LOG2)

// Spline is composed of cubic curves.
#define SPLINE_NUM_COEFFS 4

// ----------------------------------------------------------------------------
// Spline
// ----------------------------------------------------------------------------

struct curves_spline_segment {
	s64 coeffs[SPLINE_NUM_COEFFS];
};

struct curves_spline {
	struct curves_spline_segment segments[SPLINE_NUM_SEGMENTS];
};

// Calculates sample location for a given knot index.
static inline s64 curves_spline_locate_knot(int knot)
{
	int octave, segment_width_log2, segment_within_octave;
	s64 segment;

	// Origin must be in octave 0.
	BUILD_BUG_ON(SPLINE_DOMAIN_MIN_SHIFT < SPLINE_SEGMENTS_PER_OCTAVE_LOG2);

	// Sample location of knot 0 is 0.
	if (!knot)
		return 0;

	// Determine octave containing knot.
	//
	// The octave is contained in the high bits:
	//     octave = knot/SPLINE_SEGMENTS_PER_OCTAVE
	octave = knot >> SPLINE_SEGMENTS_PER_OCTAVE_LOG2;

	if (octave == 0) {
		// Handle linear zone.
		//
		// Octave 0 must extend all the way to actual 0, so it is the
		// whole linear range up to octave 1. All segments here have
		// minimum width.
		//
		// Here, segment == knot:
		//     x = segment*min_segment_width
		return (s64)knot << SPLINE_MIN_SEGMENT_WIDTH_LOG2;
	}

	// Determine segment width.
	//
	// Octave 1 has min width; width doubles per octave after.
	segment_width_log2 = SPLINE_MIN_SEGMENT_WIDTH_LOG2 + octave - 1;

	// Locate segment within octave.
	//
	// The location of the segment within the octave is contained in the
	// low bits:
	//     segment_within_octave = knot % SPLINE_SEGMENTS_PER_OCTAVE
	segment_within_octave = knot & (SPLINE_SEGMENTS_PER_OCTAVE - 1);

	// Locate segment globally.
	//
	// The sum total size of all previous octaves is the same as the
	// current octave size, so we offset the global location by 1 octave's
	// worth of segments:
	//     segment = (1 octave)*segments_per_octave + segment_within_octave
	segment = SPLINE_SEGMENTS_PER_OCTAVE | segment_within_octave;

	// x = segment*segment_width.
	return segment << segment_width_log2;
}

// Finds segment and interpolation input for x in piecewise geometric grid.
static inline void
curves_spline_piecewise_uniform_index(s64 x, s64 *segment_index, s64 *t)
{
	// Calculate the Effective Exponent (E')
	// The clamp to BIAS handles the "linear" region near zero.
	int e_raw = curves_log2_u64((u64)x);
	int e_clamped = (e_raw > SPLINE_DOMAIN_MIN_SHIFT) ?
				e_raw :
				SPLINE_DOMAIN_MIN_SHIFT;

	// Calculate Shift amount for this region
	// We want to map the segment width (2^shift) to 1.0 in t-space.
	int shift = e_clamped - SPLINE_SEGMENTS_PER_OCTAVE_LOG2;

	// Calculate Segment Index, Base_Region_Offset + Mantissa_Offset
	// The (x >> shift) part inherently handles the "linear" indexing
	// (where x is small) AND the "geometric" indexing (where x includes
	// the implicit leading 1).
	*segment_index = ((s64)(e_clamped - SPLINE_DOMAIN_MIN_SHIFT)
			  << SPLINE_SEGMENTS_PER_OCTAVE_LOG2) +
			 (x >> shift);

	// Calculate t (interpolation factor)
	// t is the remaining fraction of x scaled to the spline's fixed-point
	// format. We calc it using a shift to avoid division.
	// Mask out the bits consumed by the segment index.
	u64 mask = (1ULL << shift) - 1;
	u64 remainder = x & mask;

	// Normalize remainder to fixed-point 0..1.
	if (shift < SPLINE_FRAC_BITS)
		*t = remainder << (SPLINE_FRAC_BITS - shift);
	else
		*t = remainder >> (shift - SPLINE_FRAC_BITS);
}

// Calculates linear extension for x >= x_max.
static inline s64
curves_spline_extend_linear(const struct curves_spline *spline, s64 x)
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

// 1/2 in fixed point; used when rounding after multiplication.
#define SPLINE_FRAC_HALF (1LL << (SPLINE_FRAC_BITS - 1))

static inline s64
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

static inline s64 curves_spline_eval(const struct curves_spline *spline, s64 x)
{
	// Validate parameters.
	if (unlikely(x < 0))
		x = 0;
	if (unlikely(x >= curves_spline_locate_knot(SPLINE_NUM_SEGMENTS))) {
		return curves_spline_extend_linear(spline, x);
	}

	s64 segment_index;
	s64 t;
	curves_spline_piecewise_uniform_index(x, &segment_index, &t);

	return curves_spline_eval_segment(&spline->segments[segment_index], t);
}

#endif /* _CURVES_SPLINE_H */
