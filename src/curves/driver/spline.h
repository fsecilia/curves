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
// These are only slightly tunable, but the are not derived constants. Be very
// wary of modifying them.
// ----------------------------------------------------------------------------

// Fractional bits of the fixed-point format.
#define CURVES_SPLINE_FRAC_BITS 32

// Max Speed: domain actually covered by segments
//
// This is the max speed we have curves for. Above this is a linear extension
// of the final tangent of the final cubic.
//
// The value is tunable, but not really useful to change because it is
// dictated by the physics of hands and mice.
#define CURVES_DOMAIN_MAX_LOG2 7

// Min Resolution: width of smallest segments
//
// Below this is the subnormal zone, where everything is linear, just like
// floating point. It sets the floor where the geometric progression stops and
// the linear grid begins.
//
// This value is tunable, but not very useful to change. Every +1 you add here
// adds an entire octave of segments as determined by
// CURVES_GRID_MANTISSA_LOG2. Decreasing it makes the smallest segments of the
// grid wider, making the grid coarser, but reducing the total number of
// segments. Increasing it makes them smaller, increasing the resolution of the
// grid, but increasing the number of segments.
#define CURVES_DOMAIN_MIN_LOG2 -8

// Density: segments per octave
//
// Density describes how many cubics we stuff into a single octave. Lower
// values give wider dynamic range but faster growth. Higher values give less
// dynamic range but slower growth.
//
// This value is coarsely tunable:
// 3 = 8/octave: less accurate, fewer segments
// 4 = 16/octave: balances accuracy, number of segments
// 5 = 32/octave: more accurate, more segments
#define CURVES_GRID_MANTISSA_LOG2 4

// ----------------------------------------------------------------------------
// Derived Parameters
// ----------------------------------------------------------------------------
// These are not tunable.
// ----------------------------------------------------------------------------

// Unit conversion from CURVES_DOMAIN_MIN_LOG2 to our fixed-point format.
#define CURVES_GRID_EXPONENT_BIAS \
	(CURVES_SPLINE_FRAC_BITS + CURVES_DOMAIN_MIN_LOG2)

// Total octaves needed to span span min to max.
#define CURVES_GRID_TOTAL_OCTAVES \
	(CURVES_DOMAIN_MAX_LOG2 - CURVES_DOMAIN_MIN_LOG2)

// Total segments needed to cover all octaves.
#define CURVES_SPLINE_NUM_SEGMENTS \
	(CURVES_GRID_TOTAL_OCTAVES << CURVES_GRID_MANTISSA_LOG2)

// Spline is composed of cubic curves.
#define CURVES_SPLINE_NUM_COEFFS 4

// ----------------------------------------------------------------------------
// Spline
// ----------------------------------------------------------------------------

struct curves_spline_segment {
	s64 coeffs[CURVES_SPLINE_NUM_COEFFS];
};

struct curves_spline {
	struct curves_spline_segment segments[CURVES_SPLINE_NUM_SEGMENTS];
};

// Calculates the x location for a given segment index.
static inline s64 curves_spline_locate_knot(int index)
{
	if (index == 0)
		return 0;

	const int m = CURVES_GRID_MANTISSA_LOG2;
	const int b = CURVES_GRID_EXPONENT_BIAS;

	int block = index >> m;

	// If Block == 0 (N << (B - M))
	// Else          ((1<<M) + Rem) << (B + Block - 1 - M)

	// Linear Region (Block 0)
	// Note: The math forces Block 1 (First Octave) to share this width
	// to maintain alignment, so this branch handles both implicitly
	// if you view index simply as a linear counter up to 2^(M+1).
	if (block == 0) {
		return (s64)index << (b - m);
	}

	// Geometric Region
	// Reconstruct 1.mantissa * 2^E
	int mantissa = index & ((1 << m) - 1);
	s64 val = (1ULL << m) | mantissa;

	// Shift = E - M = (B + block - 1) - M
	int shift = (b + block - 1) - m;

	return val << shift;
}

// Finds segment and interpolation input for x in piecewise geometric grid.
static inline void
curves_spline_piecewise_uniform_index(s64 x, s64 *segment_index, s64 *t)
{
	// Ensure the grid bias doesn't create negative shifts.
	BUILD_BUG_ON(CURVES_GRID_EXPONENT_BIAS < CURVES_GRID_MANTISSA_LOG2);

	// Calculate the Effective Exponent (E')
	// The clamp to BIAS handles the "linear" region near zero.
	int e_raw = curves_log2_u64((u64)x);
	int e_clamped = (e_raw > CURVES_GRID_EXPONENT_BIAS) ?
				e_raw :
				CURVES_GRID_EXPONENT_BIAS;

	// Calculate Shift amount for this region
	// We want to map the segment width (2^shift) to 1.0 in t-space.
	int shift = e_clamped - CURVES_GRID_MANTISSA_LOG2;

	// Calculate Segment Index, Base_Region_Offset + Mantissa_Offset
	// The (x >> shift) part inherently handles the "linear" indexing
	// (where x is small) AND the "geometric" indexing (where x includes
	// the implicit leading 1).
	*segment_index = ((s64)(e_clamped - CURVES_GRID_EXPONENT_BIAS)
			  << CURVES_GRID_MANTISSA_LOG2) +
			 (x >> shift);

	// Calculate t (interpolation factor)
	// t is the remaining fraction of x scaled to the spline's fixed-point
	// format. We calc it using a shift to avoid division.
	// Mask out the bits consumed by the segment index.
	u64 mask = (1ULL << shift) - 1;
	u64 remainder = x & mask;

	// Normalize remainder to fixed-point 0..1.
	if (shift < CURVES_SPLINE_FRAC_BITS)
		*t = remainder << (CURVES_SPLINE_FRAC_BITS - shift);
	else
		*t = remainder >> (shift - CURVES_SPLINE_FRAC_BITS);
}

// Calculates linear extension for x >= x_max.
static inline s64
curves_spline_extend_linear(const struct curves_spline *spline, s64 x)
{
	s64 x_max = curves_spline_locate_knot(CURVES_SPLINE_NUM_SEGMENTS);
	int last_idx = CURVES_SPLINE_NUM_SEGMENTS - 1;

	// Calc shift (width) of that last segment to scale the slope.
	// We can infer it from the index.
	int m_bits = CURVES_GRID_MANTISSA_LOG2;
	int b_bias = CURVES_GRID_EXPONENT_BIAS;
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

	int scale = CURVES_SPLINE_FRAC_BITS - last_shift;
	s64 slope;

	if (scale >= 0)
		slope = (s64)(dy_dt << scale);
	else
		slope = (s64)(dy_dt >> (-scale));

	// result = slope * (x - x_max) + y_max
	s64 y_max = a + b + c + d;
	return (s64)(((s128)slope * (x - x_max)) >> CURVES_SPLINE_FRAC_BITS) +
	       y_max;
}

static inline s64
curves_spline_eval_segment(const struct curves_spline_segment *segment, s64 t)
{
	// Horner's loop.
	const s64 *coeff = segment->coeffs;
	s64 result = *coeff++;
	for (int i = 1; i < CURVES_SPLINE_NUM_COEFFS; ++i) {
		result = (s64)(((s128)result * t) >> CURVES_SPLINE_FRAC_BITS);
		result += *coeff++;
	}

	return result;
}

static inline s64 curves_spline_eval(const struct curves_spline *spline, s64 x)
{
	// Validate parameters.
	if (unlikely(x < 0))
		x = 0;
	if (unlikely(x >=
		     curves_spline_locate_knot(CURVES_SPLINE_NUM_SEGMENTS))) {
		return curves_spline_extend_linear(spline, x);
	}

	s64 segment_index;
	s64 t;
	curves_spline_piecewise_uniform_index(x, &segment_index, &t);

	return curves_spline_eval_segment(&spline->segments[segment_index], t);
}

#endif /* _CURVES_SPLINE_H */
