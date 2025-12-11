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

// Finds segment and interpolation input for x in piecewise sampling grid.
static inline void
curves_spline_piecewise_uniform_index(s64 x, s64 *segment_index, s64 *t)
{
	s64 bit36 = (x >> 36) & 1;
	s64 bit37 = (x >> 37) & 1;
	s64 bit38 = (x >> 38) & 1;

	s64 ge16 = bit36 | bit37 | bit38;
	s64 ge32 = bit37 | bit38;
	s64 ge64 = bit38;

	s64 base = (ge16 << 7) + (ge32 << 6) + (ge64 << 5);
	s64 shift = 29 + ge16 + (ge32 << 1) + ge64;
	s64 region_start = (ge16 + ge32 + (ge64 << 1)) << 36;

	s64 offset = x - region_start;
	*segment_index = base + (offset >> shift);

	s64 mask = (1LL << shift) - 1;
	s64 frac = offset & mask;
	*t = (frac << 3) >> (shift - 29);
}

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
		// We shift +2 because thef final bin is 1/4 the width.
		s64 m = (s64)(((s128)(3 * a + 2 * b + c) *
			       CURVES_SPLINE_INV_SEGMENT_WIDTH) >>
			      (CURVES_SPLINE_FRAC_BITS + 2));
		s64 y_max = a + b + c + d;

		// y = m(x - x_max) + y_max
		return (s64)(((s128)m * (x - CURVES_SPLINE_DOMAIN_END_FIXED)) >>
			     CURVES_SPLINE_FRAC_BITS) +
		       y_max;
	}

	s64 segment_index;
	s64 t;
	curves_spline_piecewise_uniform_index(x, &segment_index, &t);

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
