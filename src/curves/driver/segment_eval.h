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

static inline s64
curves_eval_segment(const struct curves_normalized_segment *segment, u64 t)
{
	const s64 *coeffs = segment->coeffs;
	s64 accumulator = coeffs[0];

	for (int i = 0; i < 2; ++i) {
		s128 product = (s128)accumulator * t;
		int shift = 64 + segment->relative_shifts[i];
		product += ((s128)1 << (shift - 1));
		accumulator = (s64)(product >> shift);
		accumulator += coeffs[i + 1];
	}

	s128 product = (s128)accumulator * t;

	int shift_c3 = 64 + segment->relative_shifts[2];
	int shift_final = segment->relative_shifts[3];
	int shift_prod = shift_c3 + shift_final;

	product += ((s128)1 << (shift_prod - 1));
	s64 term_prod = (s64)(product >> shift_prod);

	s64 term_c3;
	if (likely(shift_final > 0)) {
		s64 half = 1LL << (shift_final - 1);
		term_c3 = (coeffs[3] + half) >> shift_final;
	} else {
		term_c3 = coeffs[3] << (-shift_final);
	}

	return term_prod + term_c3;
}

#endif /* _CURVES_SEGMENT_EVAL_H */
