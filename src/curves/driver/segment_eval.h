// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Poly hermite spline segment evaluation.
 *
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#ifndef _CURVES_SEGMENT_EVAL_H
#define _CURVES_SEGMENT_EVAL_H

#include "kernel_compat.h"

enum {
	// Number of coefficients in polynomial.
	CURVES_SEGMENT_COEFF_COUNT = 4, // cubic

	// Precision of normalized, segment-local t input parameter when
	// evaluating, unsigned Q0.64.
	CURVES_SEGMENT_T_FRAC_BITS = 64,

	// Precision of evaluated segments, Q15.48.
	CURVES_SEGMENT_OUT_FRAC_BITS = 48,
};

/**
 * struct curves_normalized_poly - Polynomial coefficients with normalized
 * values in delta-shift form.
 * @coeffs: Coefficients in descending powers, normalized to signed Q0.45.
 * @relative_shifts: Relative shifts for coefficients used in Horner's method.
 *
 * This stores coefficients with their msb at bit 45 with full resolution
 * below. The relative shifts are how much to right shift after each Horner's
 * iteration to prepare for the next sum. The final shift aligns it to 
 * CURVES_SEGMENT_OUT_FRAC_BITS, with no normalization.
 */
struct curves_normalized_poly {
	s64 coeffs[CURVES_SEGMENT_COEFF_COUNT];
	s8 relative_shifts[CURVES_SEGMENT_COEFF_COUNT];
};

/**
 * struct curves_normalized_inv_width - Used to converts from spline x to 
 * segment t.
 * @value: Reciprocal of segment width, normalized to unsigned Q0.45.
 * @shift: Absolute shift amount for the inverse width.
 *
 * This stores value with the msb at bit 45 with full resolution below. The
 * absolute shift is how much to right shift to recover the original value 
 * after multiplication.
 */
struct curves_normalized_inv_width {
	u64 value;
	u8 shift;
};

/**
 * struct curves_normalized_segment - Segment with normalized values in 
 * delta-shift form.
*/
struct curves_normalized_segment {
	struct curves_normalized_poly poly;
	struct curves_normalized_inv_width inv_width;
};

// Horner's iteration with relative shifts.
static inline s64
__curves_segment_eval_poly_iter(const struct curves_normalized_poly *poly,
				u64 t, s64 accumulator, int i)
{
	int shift = CURVES_SEGMENT_T_FRAC_BITS + poly->relative_shifts[i - 1];
	s128 product = (s128)accumulator * t + ((s128)1 << (shift - 1));
	return (s64)(product >> shift) + poly->coeffs[i];
}

/*
 * Final Horner's iteration performs final relative shift at 128 to prevent
 * shifting right, then shifting left:
 *	((a*b >> right) + c) << left == (a*b >> (right - left)) + (c << left)
 */
static inline s64
__curves_segment_eval_poly_iter_final(const struct curves_normalized_poly *poly,
				      u64 t, s64 accumulator)
{
	int shift_final = poly->relative_shifts[3];

	int shift = 64 + poly->relative_shifts[2] + shift_final;
	s128 product = (s128)accumulator * t + ((s128)1 << (shift - 1));
	accumulator = (s64)(product >> shift);

	if (likely(shift_final > 0)) {
		s64 half = 1LL << (shift_final - 1);
		accumulator += (poly->coeffs[3] + half) >> shift_final;
	} else {
		accumulator += poly->coeffs[3] << -shift_final;
	}

	return accumulator;
}

// Runs Horner's loop to evaluate poly polynomial.
static inline s64
__curves_segment_eval_poly(const struct curves_normalized_poly *poly, u64 t)
{
	s64 accumulator = poly->coeffs[0];

	// Horner's loop with relative shifts.
	for (int i = 1; i < CURVES_SEGMENT_COEFF_COUNT - 1; ++i) {
		accumulator = __curves_segment_eval_poly_iter(poly, t,
							      accumulator, i);
	}

	// Unroll final iteration to combine final shift instead of shifting
	// right, then left.
	return __curves_segment_eval_poly_iter_final(poly, t, accumulator);
}

// Converts from x in reference frame of spline to local t in reference frame
// of segment. t has precision CURVES_SEGMENT_T_FRAC_BITS.
// Precondition x in [x0, x0 + segment_width]
static inline u64
__curves_segment_x_to_t(const struct curves_normalized_inv_width *inv_width,
			s64 x, s64 x0, unsigned int x_frac_bits)
{
	int shift = inv_width->shift + x_frac_bits - CURVES_SEGMENT_T_FRAC_BITS;
	u128 t = (u128)(x - x0) * inv_width->value;
	return (u64)(shift >= 0 ? (t >> shift) : (t << -shift));
}

// Evaluates segment polynomial in reference frame of spline.
static inline s64
curves_segment_eval(const struct curves_normalized_segment *segment, s64 x,
		    s64 x0, unsigned int x_frac_bits)
{
	return __curves_segment_eval_poly(
		&segment->poly, __curves_segment_x_to_t(&segment->inv_width, x,
							x0, x_frac_bits));
}

#endif /* _CURVES_SEGMENT_EVAL_H */
