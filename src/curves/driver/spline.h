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
	s64 coeffs[CURVES_SPLINE_NUM_SEGMENTS][CURVES_SPLINE_NUM_COEFFS];
};

static inline s64 curves_spline_eval(const struct curves_spline *spline, s64 x)
{
	// Validate parameters.
	if (unlikely(x < 0))
		x = 0;
	if (x >= CURVES_SPLINE_DOMAIN_END_FIXED) {
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
			       CURVES_SPLINE_INV_SEGMENT_WIDTH) >>
			      CURVES_SPLINE_FRAC_BITS);
		s64 y_max = a + b + c + d;

		// y = m(x - x_max) + y_max
		return (s64)(((s128)m * (x - CURVES_SPLINE_DOMAIN_END_FIXED)) >>
			     CURVES_SPLINE_FRAC_BITS) +
		       y_max;
	}

	s64 width = CURVES_SPLINE_INV_SEGMENT_WIDTH;

	// Index into segment with normalized t.
	s64 x_segment = (s64)(((s128)x * width) >> CURVES_SPLINE_FRAC_BITS);
	s64 segment_index = x_segment >> CURVES_SPLINE_FRAC_BITS;
	s64 t = x_segment & ((1LL << CURVES_SPLINE_FRAC_BITS) - 1);

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
