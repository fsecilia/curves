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

#define CURVES_SPLINE_FRAC_BITS 32

#define CURVES_SPLINE_NUM_COEFFS 4

#define CURVES_SPLINE_NUM_SEGMENTS_BITS 8
#define CURVES_SPLINE_NUM_SEGMENTS (1LL << CURVES_SPLINE_NUM_SEGMENTS_BITS)

#define CURVES_SPLINE_DOMAIN_END_BITS 7
#define CURVES_SPLINE_DOMAIN_END_INT (1LL << CURVES_SPLINE_DOMAIN_END_BITS)
#define CURVES_SPLINE_DOMAIN_END_FIXED \
	((s64)CURVES_SPLINE_DOMAIN_END_INT << CURVES_SPLINE_FRAC_BITS)

#define CURVES_SPLINE_SEGMENT_WIDTH \
	(CURVES_SPLINE_DOMAIN_END_FIXED / CURVES_SPLINE_NUM_SEGMENTS)
#define CURVES_SPLINE_INV_SEGMENT_WIDTH             \
	(s64)((((s128)CURVES_SPLINE_NUM_SEGMENTS    \
		<< (2 * CURVES_SPLINE_FRAC_BITS)) / \
	       CURVES_SPLINE_DOMAIN_END_FIXED))

struct curves_spline {
	s64 alpha; // gentleness of progressive growth
	s64 log2_alpha; // precomputed log2(alpha)
	s64 inv_log_range; // scaling factor N/log2(1 + x_max/a)
	s64 x_max; // after x_max, extend final tangent
	s64 final_segment_inv_width; // 1/(width of final segment)

	// coefficients by segment
	s64 coeffs[CURVES_SPLINE_NUM_SEGMENTS][CURVES_SPLINE_NUM_COEFFS];
};

static inline s64 curves_spline_eval(const struct curves_spline *spline, s64 x)
{
	// Validate parameters.
	if (unlikely(x < 0))
		x = 0;
	if (x >= spline->x_max) {
		// Extend final tangent.

		// Extract named cubic coefs.
		const s64 *coeff =
			spline->coeffs[CURVES_SPLINE_NUM_SEGMENTS - 1];
		s64 a = *coeff++;
		s64 b = *coeff++;
		s64 c = *coeff++;
		s64 d = *coeff++;

		// Calc slope and y at x_max.
		s64 m = (s64)(((s128)(3 * a + 2 * b + c) *
			       spline->final_segment_inv_width) >>
			      CURVES_SPLINE_FRAC_BITS);
		s64 y_max = a + b + c + d;

		// y = m(x - x_max) + y_max
		return (s64)(((s128)m * (x - spline->x_max)) >>
			     CURVES_SPLINE_FRAC_BITS) +
		       y_max;
	}

	// W(x) = N*log2(1 + x/a)/log2(1 + x_max/a)
	//      = N*(log2(a + x) - log2(a))/(log2(1 + x_max/a))
	//      = (log2(a + x) - log2(a))*inv_log_range

	s64 log2_alpha_plus_x = approx_log2_q32_q32(spline->alpha + x);
	s64 log2_diff = log2_alpha_plus_x - spline->log2_alpha;
	s64 w = (s64)(((s128)log2_diff * spline->inv_log_range) >>
		      CURVES_SPLINE_FRAC_BITS);
	s64 segment_index = w >> CURVES_SPLINE_FRAC_BITS;
	s64 t = w & ((1LL << CURVES_SPLINE_FRAC_BITS) - 1);

	if (unlikely(segment_index >= CURVES_SPLINE_NUM_SEGMENTS))
		segment_index = CURVES_SPLINE_NUM_SEGMENTS - 1;

	// Horner's loop.
	const s64 *coeff = spline->coeffs[segment_index];
	s64 result = *coeff++;
	for (int i = 1; i < CURVES_SPLINE_NUM_COEFFS; ++i) {
		result = (s64)(((s128)result * t) >> CURVES_SPLINE_FRAC_BITS);
		result += *coeff++;
	}

	return result;
}

#endif /* _CURVES_SPLINE_H */
