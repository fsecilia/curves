// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Cubic hermite spline segment unpacking.
 *
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#ifndef _CURVES_SEGMENT_UNPACKING_H
#define _CURVES_SEGMENT_UNPACKING_H

#include "kernel_compat.h"
#include "segment/eval.h"

enum {
	// Precision of normalized values, Q0.45. Some values are signed,
	// some unsigned, but they are all 45 bits wide.
	CURVES_SEGMENT_FRAC_BITS = 45,

	// Shift to right-align coefficients after extraction.
	CURVES_SEGMENT_COEFFICIENT_SHIFT = 64 - CURVES_SEGMENT_FRAC_BITS,

	// Precision of most shift integer values. Some are signed,
	// some unsigned, but they are all 6 bits wide.
	CURVES_SEGMENT_PAYLOAD_FIELD_BITS = 6,

	// Precision of final shift from internal precision to requested output
	// precision.
	CURVES_SEGMENT_PAYLOAD_TOP_BITS = 7,

	// The payload of each element in the packed array must have room for
	// 2, 6-bit shift values, and a 7-bit value, or a single 19-bit value.
	// The coefficient uses what remains.
	CURVES_SEGMENT_PAYLOAD_BITS = 2 * CURVES_SEGMENT_PAYLOAD_FIELD_BITS +
				      CURVES_SEGMENT_PAYLOAD_TOP_BITS
};

// Masks whole portion below coefficient.
#define CURVES_SEGMENT_PAYLOAD_MASK ((1ULL << CURVES_SEGMENT_PAYLOAD_BITS) - 1)

// Masks individual payload fields.
#define CURVES_SEGMENT_PAYLOAD_FIELD_MASK \
	((1ULL << CURVES_SEGMENT_PAYLOAD_FIELD_BITS) - 1)

/**
 * struct curves_packed_segment - Cubic Hermite segment packed into 32 bytes.
 * @v: Array of 4 words containing packed data.
 *
 * This structure packs 5 normalized, fixed-point values and the shifts
 * necessary to reconstruct them at their original precision. It fits exactly
 * into half of a 64-byte cache line.
 *
 * Each word packs one normalized coefficient, that coefficient's relative
 * shift in the Horner loop, and a fragment of the inverse width and its
 * absolute shift.
 *
 * The array is ordered by polynomial coefficients in descending powers (v[0]
 * corresponds to term t^3).
 *
 * Coefficients and their shifts are signed. inv_width and its shift are
 * unsigned. The final relative shift, stored with coefficient 3, is 7 bits.
 * The other shifts are 6 bits.
 *
 * Packing Layout:
 *
 *      63                           19 18                                   0
 *      +-------------~  ~-------------+-------------------------------------+
 * v[0] |         coeff 0 (45)         |       inv_width[0..18] (19)         |
 *      +-------------~  ~-------------+-------------------------------------+
 * v[1] |         coeff 1 (45)         |       inv_width[19..37] (19)        |
 *      +-------------~  ~-------------+-------------+-----------+-----------+
 * v[2] |         coeff 2 (45)         | w[38..44](7)|  sh w (6) |  sh 0 (6) |
 *      +-------------~  ~-------------+-------------+-----------+-----------+
 * v[3] |         coeff 3 (45)         |   sh 3 (7)  |  sh 2 (6) |  sh 1 (6) |
 *      +-------------~  ~-------------+-------------+-----------+-----------+
 *      63                           19 18         12 11        6 5          0
 *
 */
struct curves_packed_segment {
	u64 v[CURVES_SEGMENT_COEFF_COUNT];
} __attribute__((aligned(32)));

/**
 * curves_unpack_segment() - Unpacks a segment into normalized form.
 * @packed: Pointer to the packed segment data.
 *
 * Reconstructs the coefficients and shifts from the packed 256-bit
 * representation.
 *
 * Return: Unpacked, normalized segment.
 */
struct curves_normalized_segment
curves_unpack_segment(const struct curves_packed_segment *src);

#endif /* _CURVES_SEGMENT_UNPACKING_H */
