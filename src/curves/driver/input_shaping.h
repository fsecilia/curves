// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Input shaping functions to smoothly control entry and exit tangents.
 *
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#ifndef _CURVES_INPUT_SHAPING_H
#define _CURVES_INPUT_SHAPING_H

#include "kernel_compat.h"

// Defines shape of sextic polynomial transition between linear segments.
struct curves_conditioning_transition_poly {
	s64 v_begin; // Segment begin, in input velocity.
	s64 v_width; // Segment width, in input velocity.
	s64 v_width_inv; // `1 / v_width`
	s64 c4, c5, c6; // Polynomial Coefficients
};

// Defines placement and shape of conditioning segments.
struct curves_conditioning_params {
	s64 u_floor; // Constant output `u` from floor region.
	struct curves_conditioning_transition_poly fade;
	s64 u_lag; // Vertical shift to keep linear region continuous.
	struct curves_conditioning_transition_poly taper;
	s64 u_ceiling; // Constant output `u` from ceiling region.
};

s64 curves_conditioning_apply(
	s64 v, const struct curves_conditioning_params *conditioning);

#endif /* _CURVES_INPUT_SHAPING_H */
