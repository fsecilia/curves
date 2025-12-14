// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Cubic Hermite spline interpolation to approximate sensitivity curves.
 *
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#ifndef _CURVES_SPLINE_H
#define _CURVES_SPLINE_H

#include "kernel_compat.h"

// ----------------------------------------------------------------------------
// Tunable Parameters
// ----------------------------------------------------------------------------
// These are only slightly tunable, but the are not derived constants. Be wary
// of modifying them.
// ----------------------------------------------------------------------------

// Fractional bits of the fixed-point format.
#define SPLINE_FRAC_BITS 32

// Domain minimum.
//
// Smallest input the spline handles with full geometric resolution. Below
// this, segments have constant, minimum width.
//
// Increasing it makes the smallest segments of the wider, making the
// resolution coarser, but reducing the total number of segments. Decreasing it
// makes them smaller, making the resolution finer, but increasing the number
// of segments. Every -1 adds a whole octave's worth of segments.
#define SPLINE_DOMAIN_MIN_LOG2 -8

// Domain maximum.
//
// Largest input covered by spline segments. Above this, output extrapolates
// linearly using the final segment's slope.
//
// 2^7 = 128 exceeds typical mouse velocity. 2^6 = 64 does not.
#define SPLINE_DOMAIN_MAX_LOG2 7

// Segments per octave.
//
// How finely each octave is subdivided.
//
// Empirically, given SPLINE_DOMAIN_MIN_LOG2 == -8,
// 3 = 8/octave: less accurate, fewer segments
// 4 = 16/octave: balances accuracy, number of segments
// 5 = 32/octave: more accurate, more segments
#define SPLINE_SEGMENTS_PER_OCTAVE_LOG2 4
#define SPLINE_SEGMENTS_PER_OCTAVE (1LL << SPLINE_SEGMENTS_PER_OCTAVE_LOG2)

// ----------------------------------------------------------------------------
// Derived Parameters
// ----------------------------------------------------------------------------

// Bit position of DOMAIN_MIN in fixed-point representation.
#define SPLINE_DOMAIN_MIN_SHIFT (SPLINE_FRAC_BITS + SPLINE_DOMAIN_MIN_LOG2)

// Width of smallest segments.
// Octave 0 (linear) and octave 1 share this width; doubling starts at octave 2.
#define SPLINE_MIN_SEGMENT_WIDTH_LOG2 \
	(SPLINE_DOMAIN_MIN_SHIFT - SPLINE_SEGMENTS_PER_OCTAVE_LOG2)

// Total octaves needed to span span min to max.
#define SPLINE_NUM_OCTAVES (SPLINE_DOMAIN_MAX_LOG2 - SPLINE_DOMAIN_MIN_LOG2)

// Total segments needed to cover all octaves.
#define SPLINE_NUM_SEGMENTS \
	((SPLINE_NUM_OCTAVES + 1) << SPLINE_SEGMENTS_PER_OCTAVE_LOG2)

// Spline is composed of cubic curves.
#define SPLINE_NUM_COEFFS 4

// 1/2 in fixed point; used when rounding after multiplication.
#define SPLINE_FRAC_HALF (1LL << (SPLINE_FRAC_BITS - 1))

// ----------------------------------------------------------------------------
// Spline
// ----------------------------------------------------------------------------

struct curves_spline_segment {
	s64 coeffs[SPLINE_NUM_COEFFS];
};

struct curves_spline {
	struct curves_spline_segment segments[SPLINE_NUM_SEGMENTS];
};

s64 curves_spline_eval(const struct curves_spline *spline, s64 x);

#endif /* _CURVES_SPLINE_H */
