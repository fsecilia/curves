// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Packed and unpacked cubic Hermite spline segments.
 *
 * This module presents APIs to unpack 32-byte packed cubic Hermite spline 
 * segments and evaluate them. 
 *
 * 32 bytes gives 256 bits total to distribute nonuniformly among 5 fixed-point
 * values and their relative shifts. That's 4 cubic coefficients, the segment's
 * inverse width, and 5 shifts. 
 *
 * All shifts are 6 bits. Coefficients use signed shifts in the range [-32, 32)
 * relative to a fixed 64-bit bias on the 128-bit product. inv_width uses an
 * absolute shift in the range [0, 64).
 *
 * The approximated functions increase monotonically, so `b^2 <= 3ac` holds.
 * This means coefficients tend to be of similar order, and a 6-bit signed
 * shift tends to be sufficient. Deltas exceeding this range are capped and the
 * mantissa scaled to compensate. That gives 30 bits for shifts.
 *
 * The remaining 226 bits are split evenly among the 5 values, and the remaining
 * bit goes to inv_width. That's (45 + 6)*4 + (46 + 6) = 256.
 *
 * Packing Layout:
 *
 *      63                           19 18                                   0
 *      +-------------~  ~-------------+-------------------------------------+
 * v[0] |         coeff 0 (45)         |       inv_width[0..18] (19)         |
 *      +-------------~  ~-------------+-------------------------------------+
 *
 *
 *      63                           19 18                                   0
 *      +-------------~  ~-------------+-------------------------------------+
 * v[1] |         coeff 1 (45)         |       inv_width[19..37] (19)        |
 *      +-------------~  ~-------------+-------------------------------------+
 *
 *
 *      63                           19 18         12 11        6 5          0
 *      +-------------~  ~-------------+-------------+-----------+-----------+
 * v[2] |         coeff 2 (45)         | w[38..44](7)|  sh 1 (6) |  sh 0 (6) |
 *      +-------------~  ~-------------+-------------+-----------+-----------+
 *
 *      63                           19 18 17      12 11        6 5          0
 *      +-------------~  ~-------------+-+-----------+-----------+-----------+
 * v[3] |         coeff 3 (45)         |w|  sh w (6) |  sh 3 (6) |  sh 2 (6) |
 *      +-------------~  ~-------------+-+-----------+-----------+-----------+
 *                                      ^ w[45] (1)
 *
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#ifndef _CURVES_SEGMENT_H
#define _CURVES_SEGMENT_H

#include "kernel_compat.h"
#include "fixed.h"

// Number of coefficients in a cubic polynomial.
static const int curves_cubic_poly_coeff_count = 4;

/*
 * Packing Layout Definitions
 *
 * We define the number of bits in a shift, then require fitting 3 in one
 * packed member, plus one extra bit. Then the coefficients use all of what
 * remains. We also derive masks.
 */
enum {
	CURVES_SHIFT_BITS = 6,
	CURVES_PAYLOAD_BITS = 3 * CURVES_SHIFT_BITS + 1,
	CURVES_COEFF_BITS = 64 - CURVES_PAYLOAD_BITS,
};

static const u64 CURVES_SHIFT_MASK = (1ULL << CURVES_SHIFT_BITS) - 1;
static const u64 CURVES_PAYLOAD_MASK = (1ULL << CURVES_PAYLOAD_BITS) - 1;

/**
 * struct curves_packed_segment - Cubic Hermite segment packed into 32 bytes.
 * @v: Array of 4 words containing packed data.
 *
 * This structure holds 5 normalized, fixed-point values and the shifts
 * necessary to reconstruct them. It fits exactly into half of a 64-byte cache
 * line.
 *
 * Each word packs one normalized coefficient, that coefficient's relative
 * shift in the Horner loop, and a fragment of the inverse width and its
 * absolute shift.
 *
 * The array is ordered by polynomial coefficients in descending order (v[0]
 * corresponds to term t^3).
 *
 * Coefficients a and b are stored signed. c, d, and inv_width are stored
 * unsigned.
 */
struct curves_packed_segment {
	u64 v[4];
} __attribute__((aligned(32)));

/**
 * struct curves_normalized_segment - Unpacked segment ready for calculation.
 * @coeffs: The polynomial coefficients, a, b, c, and d.
 * @inv_width: The inverse width of the segment.
 * @relative_shifts: Shift amounts for the coefficients used in Horner's method.
 * @inv_width_shift: Absolute shift amount for the inverse width.
 */
struct curves_normalized_segment {
	s64 coeffs[4];
	u64 inv_width;
	s8 relative_shifts[4];
	u8 inv_width_shift;
};

static inline s8 __curves_sign_extend_shift(u8 value)
{
	const u64 sign_shift = BITS_PER_BYTE - CURVES_SHIFT_BITS;

	// Shift left to place sign bit in s8, then arithmetic shift back into
	// place.
	return (s8)((value & CURVES_SHIFT_MASK) << sign_shift) >> sign_shift;
}

/**
 * curves_unpack_segment() - Unpacks a segment into normalized form.
 * @packed: Pointer to the packed segment data.
 *
 * Reconstructs the coefficients and shifts from the packed 256-bit
 * representation. Handles the distribution of the scattered inv_width bits
 * across the 4 words.
 *
 * Return: Unpacked, normalized segment.
 */
static inline struct curves_normalized_segment
curves_unpack_segment(const struct curves_packed_segment *src)
{
	const int w_v2_bits = CURVES_PAYLOAD_BITS - (CURVES_SHIFT_BITS * 2);
	const int offset_v2 = CURVES_PAYLOAD_BITS * 2;
	const int offset_v3 = offset_v2 + w_v2_bits;

	struct curves_normalized_segment dst;

	// Coefficients.
	for (int i = 0; i < 4; ++i)
		dst.coeffs[i] = src->v[i] & ~CURVES_PAYLOAD_MASK;

	// Gather inverse width.
	dst.inv_width =
		(src->v[0] & CURVES_PAYLOAD_MASK) |
		(src->v[1] & CURVES_PAYLOAD_MASK) << CURVES_PAYLOAD_BITS |
		(((src->v[2] & CURVES_PAYLOAD_MASK) >> (CURVES_SHIFT_BITS * 2)))
			<< offset_v2 |
		(((src->v[3] & CURVES_PAYLOAD_MASK) >> (CURVES_SHIFT_BITS * 3)))
			<< offset_v3;

	// Shifts.
	dst.relative_shifts[0] = __curves_sign_extend_shift(src->v[2]);
	dst.relative_shifts[1] =
		__curves_sign_extend_shift(src->v[2] >> CURVES_SHIFT_BITS);
	dst.relative_shifts[2] = __curves_sign_extend_shift(src->v[3]);
	dst.relative_shifts[3] =
		__curves_sign_extend_shift(src->v[3] >> CURVES_SHIFT_BITS);
	dst.inv_width_shift = (src->v[3] >> (CURVES_SHIFT_BITS * 2)) &
			      CURVES_SHIFT_MASK;

	return dst;
}

#endif /* _CURVES_SEGMENT_H */
