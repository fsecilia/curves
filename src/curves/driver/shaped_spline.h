// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Shaped spline: transfer function with baked-in input shaping.
 *
 * This module provides a cubic Hermite spline representation of a shaped
 * transfer function T(v), where input shaping (floor, transitions, ceiling)
 * has been pre-baked into the spline during userland construction. The driver
 * simply evaluates T(v) without knowing anything about shaping.
 *
 * Key design decisions:
 *   - Adaptive curvature-based subdivision (knots placed where needed)
 *   - k-ary search with segment hints for cache-efficient lookup
 *   - Fixed-point arithmetic throughout (no floating point in kernel)
 *   - Cache-line-aligned segment storage (3 segments per 64-byte line)
 *
 * Fixed-point formats:
 *   - Knots, k-ary index: u32 Q8.24 (range [0, 256), resolution ~6e-8)
 *   - Coefficients:       s32 Q15.16 (range [-32768, 32768), resolution ~1.5e-5)
 *   - Inverse width:      s32 Q15.16 (range [-32768, 32768), resolution ~1.5e-5, for computing t)
 *   - Local parameter t:  u32 Q0.32 (value in [0, 1))
 *   - Output T(v):        s32 Q15.16
 *
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#ifndef _CURVES_SHAPED_SPLINE_H
#define _CURVES_SHAPED_SPLINE_H

#include "kernel_compat.h"

/* ----------------------------------------------------------------------------
 * Configuration
 * --------------------------------------------------------------------------*/

/* Maximum segments supported. Adaptive subdivision typically uses far fewer,
 * but we allocate for the worst case. */
#define SHAPED_SPLINE_MAX_SEGMENTS 256

/* k-ary search parameters. With fanout 9 (8 separators per level), two levels
 * cover 81 buckets, sufficient for 256 segments with ~3 segments per bucket
 * on average. */
#define SHAPED_SPLINE_KARY_FANOUT 9
#define SHAPED_SPLINE_KARY_KEYS 8
#define SHAPED_SPLINE_KARY_L1_REGIONS (SHAPED_SPLINE_KARY_FANOUT)
#define SHAPED_SPLINE_KARY_BUCKETS \
	(SHAPED_SPLINE_KARY_FANOUT * SHAPED_SPLINE_KARY_FANOUT)

/* ----------------------------------------------------------------------------
 * Fixed-Point Format Definitions
 * --------------------------------------------------------------------------*/

/* Q8.24 unsigned: knots and k-ary index values.
 * Range [0, 256), resolution 2^-24 ≈ 5.96e-8 */
#define SHAPED_SPLINE_KNOT_FRAC_BITS 24
#define SHAPED_SPLINE_KNOT_ONE (1UL << SHAPED_SPLINE_KNOT_FRAC_BITS)

/* Q15.16 signed: cubic coefficients and output.
 * Range [-32768, 32768), resolution 2^-16 ≈ 1.53e-5 */
#define SHAPED_SPLINE_COEFF_FRAC_BITS 16
#define SHAPED_SPLINE_COEFF_ONE (1L << SHAPED_SPLINE_COEFF_FRAC_BITS)

/* Q0.32 unsigned: inverse width and local parameter t.
 * Pure fraction in [0, 1), resolution 2^-32 ≈ 2.33e-10 */
#define SHAPED_SPLINE_T_FRAC_BITS 32

/* ----------------------------------------------------------------------------
 * Data Structures
 * --------------------------------------------------------------------------*/

/**
 * struct shaped_spline_segment - Cubic coefficients and inverse width.
 * @a: Cubic coefficient (t^3 term), Q15.16
 * @b: Cubic coefficient (t^2 term), Q15.16
 * @c: Cubic coefficient (t^1 term), Q15.16
 * @d: Cubic coefficient (t^0 term), Q15.16
 * @inv_width: Reciprocal of segment width, Q0.32
 *
 * Size: 20 bytes. Three of these plus 4 bytes padding fill one 64-byte
 * cache line exactly.
 *
 * The polynomial is evaluated as T(t) = ((a*t + b)*t + c)*t + d using
 * Horner's method. The inverse width is used to compute the local
 * parameter t = (v - knot) * inv_width without division.
 */
struct shaped_spline_segment {
	s32 a, b, c, d;
	u32 inv_width;
};

/**
 * struct shaped_spline - Complete shaped transfer function spline.
 * @knots: Segment boundary positions in velocity space, Q8.24.
 *         knots[i] is the start of segment i. Length is num_segments + 1.
 * @kary_l0: Level 0 of k-ary index (8 separator values), Q8.24.
 * @kary_l1: Level 1 of k-ary index (9 regions × 8 separators), Q8.24.
 * @kary_base: Starting segment index for each of 81 buckets.
 * @segments: Cache-aligned segment array.
 * @num_segments: Number of segments (at most SHAPED_SPLINE_MAX_SEGMENTS).
 * @v_max: Maximum velocity in domain, Q8.24.
 *
 * The spline represents a shaped transfer function T(v) where:
 *   - v is raw mouse velocity
 *   - T(v) is the transfer function output
 *   - Sensitivity S(v) = T(v) / v
 *   - Gain G(v) = T'(v)
 *
 * Input shaping (floor, transitions, ceiling) is pre-baked into the spline
 * during userland construction. The driver just evaluates T(v).
 */
struct shaped_spline {
	// Cache-aligned segment storage.
	struct shaped_spline_segment segments[SHAPED_SPLINE_MAX_SEGMENTS];

	// Segment boundaries for lookup and t computation. 16 per cache line
	u32 knots[SHAPED_SPLINE_MAX_SEGMENTS + 1];

	/* Two-level k-ary search index for O(1) average segment lookup.
	 * L0 fits in half a cache line, L1 spans 9 half-lines. */
	u32 kary_l0[SHAPED_SPLINE_KARY_KEYS];
	u32 kary_l1[SHAPED_SPLINE_KARY_L1_REGIONS][SHAPED_SPLINE_KARY_KEYS];
	u8 kary_base[SHAPED_SPLINE_KARY_BUCKETS];

	/* Metadata. */
	u16 num_segments;
	u32 v_max;
};

/**
 * struct shaped_spline_hint - Per-device segment lookup hint.
 * @last_segment: Segment index from most recent evaluation.
 * @valid: True if last_segment contains a valid hint.
 *
 * Mouse velocity has strong temporal coherence: consecutive samples are
 * usually in the same segment or an adjacent one. By caching the last
 * segment, we can check it and its neighbors before falling back to the
 * k-ary search, reducing cache misses from 3 lines to 1-2.
 *
 * This structure should be stored in per-device state, not in the spline
 * itself, since the spline may be shared across devices.
 */
struct shaped_spline_hint {
	u16 last_segment;
	bool valid;
};

/* ----------------------------------------------------------------------------
 * Hint Management
 * --------------------------------------------------------------------------*/

/**
 * shaped_spline_hint_init - Initialize a segment hint.
 * @hint: Hint structure to initialize.
 */
static inline void shaped_spline_hint_init(struct shaped_spline_hint *hint)
{
	hint->last_segment = 0;
	hint->valid = false;
}

/* ----------------------------------------------------------------------------
 * Lookup Functions
 * --------------------------------------------------------------------------*/

/**
 * __shaped_spline_kary_search - k-ary search fallback.
 * @spline: Spline to search.
 * @v: Velocity to locate, Q8.24.
 *
 * Two-level k-ary search with short linear scan. Guaranteed to access at
 * most 3 cache lines (1 for L0, 1 for L1, 1 for final scan in knots).
 *
 * Return: Segment index containing v.
 */
static inline int
__shaped_spline_kary_search(const struct shaped_spline *spline, u32 v)
{
	const int n = spline->num_segments;
	int r0, r1, seg;

	/* Level 0: find region (9-way branch). */
	r0 = 0;
	while (r0 < SHAPED_SPLINE_KARY_KEYS && v >= spline->kary_l0[r0])
		r0++;

	/* Level 1: find sub-region within L0 region. */
	r1 = 0;
	while (r1 < SHAPED_SPLINE_KARY_KEYS && v >= spline->kary_l1[r0][r1])
		r1++;

	/* Short linear scan from bucket base. */
	seg = spline->kary_base[r0 * SHAPED_SPLINE_KARY_FANOUT + r1];
	while (seg < n - 1 && v >= spline->knots[seg + 1])
		seg++;

	return seg;
}

/**
 * shaped_spline_find_segment - Find segment containing velocity.
 * @spline: Spline to search.
 * @hint: Per-device hint (may be NULL to skip hinting).
 * @v: Velocity to locate, Q8.24.
 *
 * First checks the hinted segment and its immediate neighbors (exploiting
 * temporal locality of mouse velocity). Falls back to k-ary search on miss.
 *
 * Cache behavior:
 *   - Hint hit (same segment): 1 cache line (knots only)
 *   - Hint hit (neighbor): 1-2 cache lines (consecutive knots)
 *   - Hint miss: 3 cache lines (k-ary L0 + L1 + knots scan)
 *
 * Return: Segment index containing v.
 */
static inline int shaped_spline_find_segment(const struct shaped_spline *spline,
					     struct shaped_spline_hint *hint,
					     u32 v)
{
	const u32 *knots = spline->knots;
	const int n = spline->num_segments;
	int seg;

	/* Try hint first: check current segment and immediate neighbors. */
	if (hint && hint->valid) {
		seg = hint->last_segment;

		/* Same segment? Most common case during smooth motion. */
		if (v >= knots[seg] && v < knots[seg + 1])
			return seg;

		/* Next segment? Accelerating. */
		if (seg + 1 < n && v >= knots[seg + 1] && v < knots[seg + 2]) {
			hint->last_segment = seg + 1;
			return seg + 1;
		}

		/* Previous segment? Decelerating. */
		if (seg > 0 && v >= knots[seg - 1] && v < knots[seg]) {
			hint->last_segment = seg - 1;
			return seg - 1;
		}
	}

	/* Hint missed or invalid: fall back to k-ary search. */
	seg = __shaped_spline_kary_search(spline, v);

	if (hint) {
		hint->last_segment = seg;
		hint->valid = true;
	}

	return seg;
}

/* ----------------------------------------------------------------------------
 * Evaluation Functions
 * --------------------------------------------------------------------------*/

/**
 * __shaped_spline_compute_t - Compute local parameter t within segment.
 * @v: Velocity, Q8.24.
 * @knot: Segment start, Q8.24.
 * @inv_width: Reciprocal of segment width, Q16.16.
 *
 * Computes t = (v - knot) / width = (v - knot) * inv_width.
 *
 * Fixed-point math:
 *   delta = v - knot                     (Q8.24)
 *   product = delta * inv_width          (Q8.24 × Q16.16 = Q24.40)
 *   t = product >> 8                     (Q0.32)
 *
 * Since v is within the segment, delta < width, so delta * inv_width < 1,
 * meaning the integer part of t is 0 and all significant bits are fractional.
 *
 * Return: Local parameter t in Q0.32.
 */
static inline u32 __shaped_spline_compute_t(u32 v, u32 knot, u32 inv_width)
{
	u32 delta = v - knot;
	u64 product = (u64)delta * inv_width;

	return (u32)(product >> 8);
}

/**
 * __shaped_spline_eval_cubic - Evaluate cubic polynomial at t.
 * @seg: Segment containing coefficients.
 * @t: Local parameter in Q0.32, value in [0, 1).
 *
 * Evaluates T(t) = ((a*t + b)*t + c)*t + d using Horner's method.
 *
 * Fixed-point math for each step:
 *   acc starts as coefficient           (Q15.16, stored in s64)
 *   acc * t                             (Q15.16 × Q0.32 = Q15.48)
 *   (acc * t) >> 32                     (Q15.16)
 *   result + next_coeff                 (Q15.16)
 *
 * We use s64 for the accumulator to avoid overflow during the multiply,
 * since Q15.16 × Q0.32 produces a Q15.48 intermediate that needs 48+ bits.
 *
 * Return: T(t) in Q15.16.
 */
static inline s32
__shaped_spline_eval_cubic(const struct shaped_spline_segment *seg, u32 t)
{
	s64 acc;

	acc = seg->a;
	acc = (acc * t) >> SHAPED_SPLINE_T_FRAC_BITS;
	acc += seg->b;

	acc = (acc * t) >> SHAPED_SPLINE_T_FRAC_BITS;
	acc += seg->c;

	acc = (acc * t) >> SHAPED_SPLINE_T_FRAC_BITS;
	acc += seg->d;

	return (s32)acc;
}

/**
 * shaped_spline_eval - Evaluate shaped transfer function at velocity.
 * @spline: Spline to evaluate.
 * @hint: Per-device segment hint (may be NULL).
 * @v: Input velocity in Q8.24.
 *
 * This is the main entry point for the driver. Given a raw mouse velocity,
 * it returns T(v), the shaped transfer function value. The caller can then
 * compute sensitivity as S = T/v or use T directly.
 *
 * The ceiling transition ensures T(v) is linear near v_max, so clamping
 * to v_max is equivalent to linear extension.
 *
 * Return: T(v) in Q15.16.
 */
static inline s32 shaped_spline_eval(const struct shaped_spline *spline,
				     struct shaped_spline_hint *hint, u32 v)
{
	const struct shaped_spline_segment *seg;
	int seg_idx;
	u32 t;

	/* Clamp to domain. The mandatory ceiling transition ensures
	 * the curve is linear at v_max, so clamping is correct behavior. */
	if (unlikely(v >= spline->v_max))
		v = spline->v_max - 1;

	/* Find segment containing v. */
	seg_idx = shaped_spline_find_segment(spline, hint, v);

	/* Get segment from cache-aligned storage. */
	seg = &spline->segments[seg_idx];

	/* Compute local parameter t ∈ [0, 1). */
	t = __shaped_spline_compute_t(v, spline->knots[seg_idx],
				      seg->inv_width);

	/* Evaluate cubic. */
	return __shaped_spline_eval_cubic(seg, t);
}

#endif /* _CURVES_SHAPED_SPLINE_H */
