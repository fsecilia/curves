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

#define CURVES_SPLINE_NUM_COEFS 4
static inline s64 curves_eval_spline(const s64 *coeffs, s64 x,
				     unsigned int x_frac_bits)
{
	s128 acc = coeffs[3];
	acc = coeffs[2] + ((acc * (s128)x) >> x_frac_bits);
	acc = coeffs[1] + ((acc * (s128)x) >> x_frac_bits);
	acc = coeffs[0] + ((acc * (s128)x) >> x_frac_bits);
	return (s64)acc;
}

struct curves_spline_max {
	s64 scale;
	s64 x;
	s64 y;
	s64 m;
};

#define CURVES_SPLINE_TABLE_SIZE 256LL
struct curves_spline_table {
	struct curves_spline_max max;
	s64 coeffs[CURVES_SPLINE_TABLE_SIZE][CURVES_SPLINE_NUM_COEFS]
		__attribute__((aligned(64)));
};

static inline s64
curves_eval_spline_table(const struct curves_spline_table table, s64 x,
			 unsigned int x_frac_bits)
{
	s128 x_scaled = ((s128)x * table.max.scale) >> x_frac_bits;
	s64 x_index = (s64)(x_scaled >> x_frac_bits);
	s64 x_eval = x_scaled & ((1LL << x_frac_bits) - 1);
	return curves_eval_spline(table.coeffs[x_index], x_eval, x_frac_bits);
}

#endif /* _CURVES_SPLINE_H */
