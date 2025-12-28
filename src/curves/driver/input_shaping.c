// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#include "input_shaping.h"
#include "fixed.h"

// Evaluates `P(t) = t^4 * (c4 + t*c5 + t^2*c6)`, with rounding, using fma.
static inline s64
evaluate_transition_poly(const struct curves_conditioning_transition_poly *p,
			 s64 t)
{
	s64 acc = p->c6;
	acc = curves_fixed_fma_round(acc, t, p->c5); // (c6 * t) + c5
	acc = curves_fixed_fma_round(acc, t, p->c4); // (Previous * t) + c4

	s64 t2 = curves_fixed_multiply_round(t, t);
	s64 t4 = curves_fixed_multiply_round(t2, t2);

	return curves_fixed_multiply_round(t4, acc);
}

// Calculates change in `u` relative to the start of transition,
// `u_rel = m_begin*v_rel + (m_end - m_begin)*v_width*P(t)`
// Assumes `v` is within transition range `[begin, begin + width)`.
static inline s64
apply_transition(const struct curves_conditioning_transition_poly *p, s64 v,
		 s64 m_begin, s64 m_end)
{
	s64 v_rel = v - p->v_begin;

	s64 t = curves_fixed_multiply_round(v_rel, p->v_width_inv);
	s64 u_poly = evaluate_transition_poly(p, t);

	s64 u_height = curves_fixed_multiply_round(m_end - m_begin, p->v_width);
	s64 u = curves_fixed_multiply_round(u_height, u_poly);

	s64 u_begin = curves_fixed_multiply_round(m_begin, v_rel);

	return u_begin + u;
}

s64 curves_conditioning_apply(
	s64 v, const struct curves_conditioning_params *conditioning)
{
	s64 v_segment_end;

	// Floor segment (m = 0).
	v_segment_end = conditioning->fade.v_begin;
	if (v < v_segment_end) {
		return conditioning->u_floor;
	}

	// Fade segment (m = 0->1).
	v_segment_end += conditioning->fade.v_width;
	if (v < v_segment_end) {
		s64 u_base = conditioning->u_floor;
		s64 u_rel = apply_transition(&conditioning->fade, v, 0,
					     CURVES_FIXED_ONE);
		return u_base + u_rel;
	}

	// Linear segment (m = 1).
	v_segment_end = conditioning->taper.v_begin;
	if (v < v_segment_end) {
		return v - conditioning->u_lag;
	}

	// Taper segment (m = 1->0).
	v_segment_end += conditioning->taper.v_width;
	if (v < v_segment_end) {
		s64 u_base = conditioning->taper.v_begin - conditioning->u_lag;
		s64 u_rel = apply_transition(&conditioning->taper, v,
					     CURVES_FIXED_ONE, 0);
		return u_base + u_rel;
	}

	// Ceiling segment (m = 0).
	return conditioning->u_ceiling;
}
