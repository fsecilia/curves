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

// Unpacks coefficient and restores implicit 1.
static inline s64 __curves_unpack_coeff(u64 raw_shifted)
{
	// Restore implicit 1.
	u64 abs_val = raw_shifted | (1ULL << CURVES_COEFF_IMPLICIT_BIT_IDX);

	// Extract sign bit.
	u64 sign_bit = raw_shifted >> CURVES_COEFF_SIGN_BIT_IDX;

	// Arithmetic conditional negation.
	s64 val = (s64)((abs_val ^ -(s64)sign_bit) + sign_bit);

	// cmov to check against zero.
	return (raw_shifted == 0) ? 0 : val;
}

/**
 * curves_unpack_segment() - Unpacks a segment into normalized form.
 * @packed: Pointer to the packed segment data.
 *
 * Reconstructs the coefficients and shifts from the packed 256-bit
 * representation.
 *
 * Return: Unpacked, normalized segment.
 */
static inline struct curves_normalized_segment
curves_unpack_segment(const struct curves_packed_segment *src)
{
	struct curves_normalized_segment dst;
	u64 payload[CURVES_SEGMENT_COEFF_COUNT];

	// 1. Extract Payloads (Bottom 19 bits)
	for (int i = 0; i < (int)CURVES_SEGMENT_COEFF_COUNT; ++i) {
		payload[i] = src->v[i] &
			     ((1ULL << CURVES_SEGMENT_COEFF_SHIFT) - 1);
	}

	// 2. Reconstruct inverse width (Standard)
	dst.inv_width.value = payload[0] | (payload[1] << 19) |
			      ((payload[2] >> 12) << 38) |
			      ((payload[3] >> 18) << 45);
	dst.inv_width.value |= (1ULL << CURVES_INV_WIDTH_IMPLICIT_BIT_IDX);

	// 3. Extract Exponents
	dst.poly.exponents[0] = payload[2] & 0x3F;
	dst.poly.exponents[1] = payload[3] & 0x3F;
	dst.poly.exponents[2] = (payload[3] >> 6) & 0x3F;
	dst.poly.exponents[3] = (payload[3] >> 12) & 0x3F;
	dst.inv_width.shift = (payload[2] >> 6) & 0x3F;

	// 4A. Unpack a, b (Signed)
	for (int i = 0; i < 2; ++i) {
		u64 raw_storage = src->v[i] >> CURVES_SEGMENT_COEFF_SHIFT;
		u8 raw_exp = dst.poly.exponents[i];

		// Signed Logic (Implicit at 44)
		u64 sign_bit = raw_storage >> 44;
		u64 mantissa = raw_storage & ((1ULL << 44) - 1);

		u64 is_denorm = (u64)(raw_exp + 1) >> 6;
		dst.poly.exponents[i] = raw_exp - (u8)is_denorm;

		u64 implicit_val = (is_denorm ^ 1ULL) << 44;
		u64 abs_val = mantissa | implicit_val;

		dst.poly.coeffs[i] =
			(raw_storage == 0) ?
				0 :
				(s64)((abs_val ^ -(s64)sign_bit) + sign_bit);
	}

	// 4B. Unpack c, d (Unsigned)
	for (int i = 2; i < 4; ++i) {
		u64 raw_storage = src->v[i] >> CURVES_SEGMENT_COEFF_SHIFT;
		u8 raw_exp = dst.poly.exponents[i];

		// Unsigned Logic (Implicit at 45)
		// We utilize the full 45 bits (0..44) for mantissa.
		u64 mantissa = raw_storage; // No sign bit to strip!

		u64 is_denorm = (u64)(raw_exp + 1) >> 6;
		dst.poly.exponents[i] = raw_exp - (u8)is_denorm;

		// Restore Implicit 1 at bit 45
		u64 implicit_val = (is_denorm ^ 1ULL) << 45;

		// No sign application needed
		dst.poly.coeffs[i] = (s64)(mantissa | implicit_val);

		// Handle exact zero
		if (raw_storage == 0)
			dst.poly.coeffs[i] = 0;
	}

	return dst;
}

#endif /* _CURVES_SEGMENT_UNPACKING_H */
