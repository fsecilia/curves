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

#define CURVES_SPLINE_SEGMENT_BITS 8
#define CURVES_SPLINE_NUM_SEGMENTS (1LL << CURVES_SPLINE_SEGMENT_BITS)

struct curves_spline {
	s64 inv_segment_width;
	s64 x_max;
	s64 coeffs[CURVES_SPLINE_NUM_SEGMENTS][CURVES_SPLINE_NUM_COEFFS];
};

static inline s64 curves_eval_spline(const s64 *coeffs, s64 x)
{
	s128 acc = coeffs[3];
	acc = coeffs[2] + ((acc * (s128)x) >> CURVES_SPLINE_FRAC_BITS);
	acc = coeffs[1] + ((acc * (s128)x) >> CURVES_SPLINE_FRAC_BITS);
	acc = coeffs[0] + ((acc * (s128)x) >> CURVES_SPLINE_FRAC_BITS);
	return (s64)acc;
}

struct curves_spline_table {
	s64 x_scale;
	s64 x_max;
	s64 y_max;
	s64 m_max;
	s64 coeffs[CURVES_SPLINE_NUM_SEGMENTS][CURVES_SPLINE_NUM_COEFFS]
		__attribute__((aligned(64)));
};

static inline s64
curves_eval_spline_table(const struct curves_spline_table *table, s64 x)
{
	s128 x_scaled = ((s128)x * table->x_scale) >> CURVES_SPLINE_FRAC_BITS;
	if (unlikely(x > table->x_max)) {
		return table->y_max +
		       ((table->m_max * (x_scaled - table->x_max)) >>
			CURVES_SPLINE_FRAC_BITS);
	}

	s64 x_index = (s64)(x_scaled >> CURVES_SPLINE_FRAC_BITS);
	s64 x_eval = x_scaled & ((1LL << CURVES_SPLINE_FRAC_BITS) - 1);
	return curves_eval_spline(table->coeffs[x_index], x_eval);
}

#endif /* _CURVES_SPLINE_H */
