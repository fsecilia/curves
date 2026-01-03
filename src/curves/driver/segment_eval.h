// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Cubic hermite spline segment evaluation.
 *
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#ifndef _CURVES_SEGMENT_EVAL_H
#define _CURVES_SEGMENT_EVAL_H

#include "kernel_compat.h"

enum {
	// Number of coefficients in a cubic polynomial.
	CURVES_SEGMENT_COEFF_COUNT = 4,
};

/**
 * struct curves_normalized_segment - Unpacked segment ready for evaluation.
 * @coeffs: Cubic coefficients in descending powers. Signed Q0.45.
 * @inv_width: Inverse width of the segment. Unsigned Q0.45.
 * @relative_shifts: Relative shifts for coefficients used in Horner's method.
 * @inv_width_shift: Absolute shift amount for the inverse width.
 */
struct curves_normalized_segment {
	s64 coeffs[CURVES_SEGMENT_COEFF_COUNT];
	u64 inv_width;
	s8 relative_shifts[CURVES_SEGMENT_COEFF_COUNT];
	u8 inv_width_shift;
};

// Horner's iteration with relative shifts.
static inline s64 __curves_eval_segment_relative_horner(
	const struct curves_normalized_segment *segment, u64 t, s64 accumulator,
	int i)
{
	int shift = 64 + segment->relative_shifts[i - 1];
	s128 product = (s128)accumulator * t + ((s128)1 << (shift - 1));
	return (s64)(product >> shift) + segment->coeffs[i];
}

/*
 * Final Horner's iteration performs final relative shift at 128 to prevent
 * shifting right, then shifting left:
 *	((a*b >> right) + c) << left == (a*b >> (right - left)) + (c << left)
 */
static inline s64 __curves_eval_segment_final_horner(
	const struct curves_normalized_segment *segment, u64 t, s64 accumulator)
{
	int shift_final = segment->relative_shifts[3];

	int shift = 64 + segment->relative_shifts[2] + shift_final;
	s128 product = (s128)accumulator * t + ((s128)1 << (shift - 1));
	accumulator = (s64)(product >> shift);

	if (likely(shift_final > 0)) {
		s64 half = 1LL << (shift_final - 1);
		accumulator += (segment->coeffs[3] + half) >> shift_final;
	} else {
		accumulator += segment->coeffs[3] << -shift_final;
	}

	return accumulator;
}

static inline s64
curves_eval_segment(const struct curves_normalized_segment *segment, u64 t)
{
	s64 accumulator = segment->coeffs[0];

	// Horner's loop with relative shifts.
	for (int i = 1; i < CURVES_SEGMENT_COEFF_COUNT - 1; ++i) {
		accumulator = __curves_eval_segment_relative_horner(
			segment, t, accumulator, i);
	}

	// Unroll final iteration to combine final shift instead of shifting
	// right, then left.
	return __curves_eval_segment_final_horner(segment, t, accumulator);
}

#endif /* _CURVES_SEGMENT_EVAL_H */
