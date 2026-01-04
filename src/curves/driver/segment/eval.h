// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Cubic Hermite spline segment evaluation.
 *
 * Copyright (C) 2026 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#ifndef _CURVES_SEGMENT_EVAL_H
#define _CURVES_SEGMENT_EVAL_H

#include "kernel_compat.h"

enum {
	// Number of coefficients in polynomial.
	CURVES_SEGMENT_COEFF_COUNT = 4,

	// Precision of normalized, segment-local t parameter, unsigned Q0.64.
	CURVES_SEGMENT_T_FRAC_BITS = 64,

	// Precision of evaluated output, Q15.48.
	CURVES_SEGMENT_OUT_FRAC_BITS = 48,
};

/**
 * struct curves_normalized_poly - Polynomial with normalized coefficients.
 * @coeffs: Coefficients in descending powers (a, b, c, d for at^3+bt^2+ct+d).
 *          Signed (a,b) have implicit 1 at bit 44.
 *          Unsigned (c,d) have implicit 1 at bit 45.
 * @shifts: Right-shift amounts to recover original scale after multiplication.
 *          Value 63 (CURVES_DENORMAL_SHIFT) indicates no implicit 1.
 */
struct curves_normalized_poly {
	s64 coeffs[CURVES_SEGMENT_COEFF_COUNT];
	u8 shifts[CURVES_SEGMENT_COEFF_COUNT];
};

/**
 * struct curves_normalized_inv_width - Normalized inverse segment width.
 * @value: Reciprocal of segment width, implicit 1 at bit 46.
 * @shift: Right-shift to recover original scale after multiplication.
 */
struct curves_normalized_inv_width {
	u64 value;
	u8 shift;
};

/**
 * struct curves_normalized_segment - Composed normalized segment.
 * @poly: Polynomial coefficients and shifts.
 * @inv_width: Inverse width for x-to-t conversion.
 */
struct curves_normalized_segment {
	struct curves_normalized_poly poly;
	struct curves_normalized_inv_width inv_width;
};

/*
 * Horner's method iteration.
 *
 * Calculates: accumulator = (accumulator * t + coeff[i]) with proper alignment.
 * The shift aligns the product to the next coefficient's scale.
 */
static inline s64
__curves_segment_eval_poly_iter(const struct curves_normalized_poly *poly,
				u64 t, s64 accumulator, int i, int prev_shift)
{
	int shift = prev_shift + CURVES_SEGMENT_T_FRAC_BITS - poly->shifts[i];

	// Multiply and add rounding bias.
	s128 product = (s128)accumulator * t + ((s128)1 << (shift - 1));

	// Shift to align and add next coefficient.
	return (s64)(product >> shift) + poly->coeffs[i];
}

/*
 * Final Horner's iteration with output alignment.
 *
 * Aligns result to CURVES_SEGMENT_OUT_FRAC_BITS rather than next coefficient.
 */
static inline s64
__curves_segment_eval_poly_iter_final(const struct curves_normalized_poly *poly,
				      u64 t, s64 accumulator, int prev_shift)
{
	int product_shift = prev_shift + CURVES_SEGMENT_T_FRAC_BITS -
			    CURVES_SEGMENT_OUT_FRAC_BITS;

	s128 product = (s128)accumulator * t + ((s128)1 << (product_shift - 1));

	// Scale final coefficient to output precision.
	int c3_shift = poly->shifts[3] - CURVES_SEGMENT_OUT_FRAC_BITS;
	s64 c3 = poly->coeffs[3];

	if (c3_shift > 0) {
		// Round up before shifting down to final output format.
		s64 half = 1LL << (c3_shift - 1);
		c3 = (c3 + half) >> c3_shift;
	} else {
		// Shift up to final output format.
		c3 = c3 << -c3_shift;
	}

	return (s64)(product >> product_shift) + c3;
}

/*
 * Evaluates polynomial using Horner's method.
 *
 * P(t) = ((a*t + b)*t + c)*t + d
 */
static inline s64
__curves_segment_eval_poly(const struct curves_normalized_poly *poly, u64 t)
{
	s64 acc = poly->coeffs[0];
	int shift = poly->shifts[0];

	acc = __curves_segment_eval_poly_iter(poly, t, acc, 1, shift);
	shift = poly->shifts[1];

	acc = __curves_segment_eval_poly_iter(poly, t, acc, 2, shift);
	shift = poly->shifts[2];

	return __curves_segment_eval_poly_iter_final(poly, t, acc, shift);
}

/*
 * Converts spline x to segment-local t.
 *
 * t = (x - x0) * inv_width, normalized to Q0.64.
 */
static inline u64
__curves_segment_x_to_t(const struct curves_normalized_inv_width *inv_width,
			s64 x, s64 x0, unsigned int x_frac_bits)
{
	int shift = inv_width->shift + x_frac_bits - CURVES_SEGMENT_T_FRAC_BITS;
	u128 t = (u128)(x - x0) * inv_width->value;
	return (u64)(shift >= 0 ? (t >> shift) : (t << -shift));
}

/*
 * Evaluates segment at position x.
 */
static inline s64
curves_segment_eval(const struct curves_normalized_segment *segment, s64 x,
		    s64 x0, unsigned int x_frac_bits)
{
	u64 t = __curves_segment_x_to_t(&segment->inv_width, x, x0,
					x_frac_bits);
	return __curves_segment_eval_poly(&segment->poly, t);
}

#endif /* _CURVES_SEGMENT_EVAL_H */
