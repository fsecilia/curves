// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * input_shaping.h - Cascaded input velocity shaping.
 *
 * This module covers the first part of the input scalar transform:
 *
 *     [v -> v' -> u] -> x -> t -> y
 *
 * See scalar_transform.h for the complete transform.
 *
 * It implements a two-stage cascade that shapes raw input velocity before it
 * is used to evaluate the configured curve shape:
 *
 *       v --> Stage 1 (Ease-In) --> v' --> Stage 2 (Ease-Out) --> u
 *
 *                                   |                  ________
 *                            /      |              .-''
 *                          /        |            /
 *    Stage 1 (Ease-In)   /          |          /    Stage 2 (Ease-Out)
 *                      /            |        /
 *        _________..-'              |      /
 *          floor |----| linear      |      linear |----| ceiling
 *              transition           |           transition
 *                                   |
 *
 * Each stage is independent; they compose functionally: stage2(stage1(v))
 *
 * The floor and transition of stage 1 are both optional. All of stage 2 is
 * mandatory.
 *
 * The entire function exhibits C3 continuity because the transition segments
 * are C3 continuous, and any scaling we apply is uniform.
 *
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#ifndef _CURVES_SHAPING_H
#define _CURVES_SHAPING_H

#include "kernel_compat.h"
#include "fixed.h"

/* ------------------------------------------------------------------------- *
 * Data Structures                                                           *
 * ------------------------------------------------------------------------- */

/*
 * Polynomial Transitions
 * ----------------------
 * To match position and slope, both transitions use the integral of
 * smootherstep, a sextic polynomial:
 *
 *   `P(t) = t^6 - 3*t^5 + 2.5*t^4 = t^4*(t^2 - 3*t + 2.5)`
 *
 * We then translate and scale this segment uniformly to match the adjacent
 * linear segments. Continuity is preserved under uniform scaling because
 * P''(0), P''(1), P'''(0), and P'''(1) are all zero by construction.
 *
 * The polynomial output is in [0, 0.5]. Because the coefficients and range are
 * small, this calc uses a higher-precision fixed-point format internally.
 */

// Shaping polynomial fixed format is Q2.61.
static const s64 curves_shaping_frac_bits = 61;
static const s64 curves_shaping_one = 1LL << curves_shaping_frac_bits;
static const s64 curves_shaping_half = 1LL << (curves_shaping_frac_bits - 1);

/*
 * Describes domain of a transition segment.
 */
struct curves_shaping_transition {
	s64 v_begin; // Segment begin, in input velocity.
	s64 v_width; // Segment width, in input velocity.
	s64 v_width_inv; // Precomputed `1 / v_width`
};

/*
 * Stage 1: Ease-In (optional)
 * ---------------------------
 * This stage shapes the low-velocity region. It has three segments:
 *
 *   1. Floor:      v' = u_floor, a constant, when v < v_begin
 *   2. Transition: v' smooth ramp from floor to linear using sextic poly
 *   3. Linear:     v' = v - lag when v >= v_begin + v_width
 *
 * This stage is optional, and the widths of the floor and transition segments
 * are configurable. It is a complete passthrough when u_floor, v_begin, and
 * v_width are all equal to 0.
 */
struct curves_shaping_ease_in {
	s64 u_floor; // Constant output during floor region.
	s64 u_lag; // Offset for linear region.
	struct curves_shaping_transition transition;
};

/*
 * Stage 2: Ease-Out (mandatory)
 * -----------------------------
 * This stage shapes the high-velocity region. It has three segments:
 *
 *   1. Linear:     u = v' (identity) when v' < v_begin
 *   2. Transition: u smooth ramp from linear to ceiling using sextic poly
 *   3. Ceiling:    u = u_ceiling, a constant, when v' >= v_begin + v_width
 *
 * This stage is mandatory. It acts as a final limiter for the mapped spline
 * domain. By the time its output reaches the end of the configured spline, the
 * output is constant and can be clamped to the final input covered by the
 * spline. The width can be made very small near the end of the range, but it
 * must exist for correctness since we're using a table.
 *
 * Stage 2's thresholds are in v' space, the output of stage 1, not raw v
 * space. Their values are specified independently, so the stages are decoupled.
 */
struct curves_shaping_ease_out {
	s64 u_ceiling; // Constant output during ceiling region.
	struct curves_shaping_transition transition;
};

// Defines placement and shape of both stages.
struct curves_shaping_params {
	struct curves_shaping_ease_in ease_in;
	struct curves_shaping_ease_out ease_out;
};

/* ------------------------------------------------------------------------- *
 * Polynomial Evaluation                                                     *
 * ------------------------------------------------------------------------- */

// High precision multiply with rounding.
static inline s64 curves_shaping_multiply_round(s64 multiplicand,
						s64 multiplier)
{
	s128 product = (s128)multiplicand * multiplier;
	return (s64)((product + curves_shaping_half) >>
		     curves_shaping_frac_bits);
}

/*
 * Evaluate the normalized shaping polynomial P(t) in high precision.
 *
 * Input:  t in [0, 1)
 * Output: P(t) in [0, 0.5]
 *
 *     `P(t) = ((c0*t + c1)*t + c2)*t^4`
 */
static inline s64 curves_shaping_eval_poly(s64 t)
{
	// Coefficients in Q2.61.
	const s64 c0 = curves_shaping_one; // 1.0L
	const s64 c1 = -6917529027641081856LL; // 3.0L
	const s64 c2 = 5764607523034234880LL; // 2.5L

	s64 inner, t2, t4;

	// Unroll Horner's loop for inner cubic. `inner = (c0*t + c1)*t + c2`
	inner = curves_shaping_multiply_round(c0, t) + c1;
	inner = curves_shaping_multiply_round(inner, t) + c2;

	// Combine exponents.
	t2 = curves_shaping_multiply_round(t, t);
	t4 = curves_shaping_multiply_round(t2, t2);

	// P(t) = inner*t4.
	return curves_shaping_multiply_round(inner, t4);
}

/*
 * Compute transition height: v_width*P(t) where t = v_rel / v_width.
 *
 * This function calculates transition height based on velocity, relative to
 * transition width. It performs domain projection (v -> t) and range scaling
 * (P(t) -> height).
 *
 * Input: velocity relative to given transition begin and the transition width
 * Output: height of the transition
 *
 * Internally uses high precision format for polynomial evaluation.
 */
static inline s64 curves_shaping_eval_transition(s64 v_rel, s64 v_width,
						 s64 v_width_inv)
{
	// Conversion constants.
	const s64 sp_128_to_hp_shift =
		CURVES_FIXED_SHIFT * 2 - curves_shaping_frac_bits;
	const s64 sp_128_to_hp_half = 1LL << (sp_128_to_hp_shift - 1);
	const s64 hp_to_sp_shift =
		curves_shaping_frac_bits - CURVES_FIXED_SHIFT;
	const s64 hp_to_sp_half = 1LL << (hp_to_sp_shift - 1);

	s64 t, poly_hp, poly_sp;

	// Project domain: v_rel -> t
	t = (s64)(((s128)v_rel * v_width_inv + sp_128_to_hp_half) >>
		  sp_128_to_hp_shift);

	// Evaluate polynomial.
	poly_hp = curves_shaping_eval_poly(t);

	// Project range: P(t) -> height
	poly_sp = (poly_hp + hp_to_sp_half) >> hp_to_sp_shift;

	// Scale by width.
	return curves_fixed_multiply_round(v_width, poly_sp);
}

/* ------------------------------------------------------------------------- *
 * Stages                                                                    *
 * ------------------------------------------------------------------------- */

/*
 * Apply stage 1 (ease-in) to raw velocity v.
 *
 * Input: v, raw input velocity
 * Output: v', intermediate velocity for stage 2
 */
static inline s64
curves_shaping_apply_ease_in(s64 v, const struct curves_shaping_ease_in *p)
{
	s64 v_rel, transition;

	if (v < p->transition.v_begin)
		return p->u_floor;

	if (v >= p->transition.v_begin + p->transition.v_width)
		return v - p->u_lag;

	v_rel = v - p->transition.v_begin;
	transition = curves_shaping_eval_transition(
		v_rel, p->transition.v_width, p->transition.v_width_inv);

	return p->u_floor + transition;
}

/*
 * Apply stage 2 (ease-out) to intermediate velocity v'.
 *
 * Input: v', intermediate velocity output from stage 1
 * Output: u, the final shaped velocity for spline input
 */
static inline s64
curves_shaping_apply_ease_out(s64 v_prime,
			      const struct curves_shaping_ease_out *p)
{
	s64 v_rel, transition;

	if (v_prime < p->transition.v_begin)
		return v_prime;

	if (v_prime >= p->transition.v_begin + p->transition.v_width)
		return p->u_ceiling;

	v_rel = v_prime - p->transition.v_begin;
	transition = curves_shaping_eval_transition(
		v_rel, p->transition.v_width, p->transition.v_width_inv);

	return p->transition.v_begin + v_rel - transition;
}

/* ------------------------------------------------------------------------- *
 * Composed Input Shaping Pipeline                                           *
 * ------------------------------------------------------------------------- */

/*
 * Apply complete input shaping: v -> v' -> u.
 */
static inline s64
curves_shaping_apply(s64 v, const struct curves_shaping_params *params)
{
	// Stage 1: ease-in (optional)
	s64 v_prime = curves_shaping_apply_ease_in(v, &params->ease_in);

	// Stage 2: ease-out (mandatory)
	return curves_shaping_apply_ease_out(v_prime, &params->ease_out);
}

#endif /* _CURVES_SHAPING_H */
