// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Cubic Hermite spline interpolation to approximate transfer functions.
 *
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#ifndef _CURVES_SPLINE_H
#define _CURVES_SPLINE_H

#include "kernel_compat.h"
#include "fixed.h"

// ----------------------------------------------------------------------------
// Tunable Parameters
// ----------------------------------------------------------------------------
// These are only slightly tunable, but the are not derived constants. Be wary
// of modifying them.
// ----------------------------------------------------------------------------

// Origin of geometric octaves.
//
// Smallest input the spline handles geometrically. Below this, segments have
// constant, minimum width.
//
// Increasing this makes the smallest segments of the wider, making the
// resolution coarser, but reducing the total number of segments. Decreasing it
// makes them smaller, making the resolution finer, but increasing the number
// of segments. Every -1 adds a whole octave's worth of segments.
#define SPLINE_OCTAVE_ORIGIN_LOG2 -7

// Domain maximum.
//
// Largest input covered by spline segments. Above this, output extrapolates
// linearly using the final segment's slope.
//
// 2^7 = 128 exceeds typical mouse velocity. 2^6 = 64 does not.
#define SPLINE_X_END_LOG2 7
#define SPLINE_X_END_MAX (1 << SPLINE_X_END_LOG2)

// Segments per octave.
//
// How finely each octave is subdivided.
//
// Empirically, given SPLINE_DOMAIN_LOG2 == [-7, 8),
// 3 = 8/octave: more accurate, more segments, 65 segments, ~4kB
// 4 = 16/octave: less accurate, fewer segments, 129 segments, ~8kB
// 5 = 32/octave: balances accuracy, number of segments, 257 segments, ~16kB
#define SPLINE_SEGMENTS_PER_OCTAVE_LOG2 4
#define SPLINE_SEGMENTS_PER_OCTAVE (1LL << SPLINE_SEGMENTS_PER_OCTAVE_LOG2)

// ----------------------------------------------------------------------------
// Derived Parameters
// ----------------------------------------------------------------------------

// Bit position of octave 0 in fixed-point representation.
#define SPLINE_OCTAVE_ORIGIN_FIXED_SHIFT \
	(CURVES_FIXED_SHIFT + SPLINE_OCTAVE_ORIGIN_LOG2)

// Width of smallest segments.
// The subnormal zone and octave 0 share this width; doubling starts after
// octave 1.
#define SPLINE_MIN_SEGMENT_WIDTH_LOG2 \
	(SPLINE_OCTAVE_ORIGIN_FIXED_SHIFT - SPLINE_SEGMENTS_PER_OCTAVE_LOG2)

// Total octaves needed to span span min to max.
#define SPLINE_NUM_OCTAVES (SPLINE_X_END_LOG2 - SPLINE_OCTAVE_ORIGIN_LOG2)

// Total segments needed to cover all octaves.
#define SPLINE_NUM_SEGMENTS \
	(s64)((SPLINE_NUM_OCTAVES + 1) << SPLINE_SEGMENTS_PER_OCTAVE_LOG2)

// Spline is composed of cubic curves.
#define SPLINE_NUM_COEFFS 4

// ----------------------------------------------------------------------------
// Spline
// ----------------------------------------------------------------------------

struct curves_segment_desc {
	s64 index;
	unsigned int width_log2;
};

struct curves_spline_coords {
	s64 segment_index, t;
};

struct curves_spline_segment {
	s64 coeffs[SPLINE_NUM_COEFFS];
};

struct curves_spline {
	// Scale factor of coordinate transform to convert from velocities in
	// physical space to position in reference space.
	s64 v_to_x;

	// End of geometric progression, start of runout.
	s64 x_geometric_limit;

	// End of runout, start of linear extension.
	s64 x_runout_limit;

	// Power-of-2 width for the runout.
	unsigned int runout_width_log2;

	// Final runout segment to bleed off curavture before linear extension.
	struct curves_spline_segment runout_segment;

	// Cubic spline segments in ABCD order.
	struct curves_spline_segment segments[SPLINE_NUM_SEGMENTS];
};

// Evaluates spline given input velocity.
s64 curves_spline_eval(const struct curves_spline *spline, s64 v);

#endif /* _CURVES_SPLINE_H */
