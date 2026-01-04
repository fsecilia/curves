// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Cubic Hermite spline segment unpacking.
 *
 * Copyright (C) 2026 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#ifndef _CURVES_SEGMENT_UNPACKING_H
#define _CURVES_SEGMENT_UNPACKING_H

#include "kernel_compat.h"
#include "segment/eval.h"

enum {
	// Packed storage: 45 bits per coefficient, 19 bits payload.
	CURVES_SEGMENT_COEFF_STORAGE_BITS = 45,
	CURVES_SEGMENT_COEFF_SHIFT = 64 - CURVES_SEGMENT_COEFF_STORAGE_BITS,

	// Signed coefficients (a, b): implicit 1 at bit 44, sign at bit 44.
	CURVES_COEFF_SIGNED_IMPLICIT_BIT = 44,
	CURVES_COEFF_SIGN_BIT = 44,

	// Unsigned coefficients (c, d): implicit 1 at bit 45.
	CURVES_COEFF_UNSIGNED_IMPLICIT_BIT = 45,

	// Inverse width: implicit 1 at bit 46, 46 bits mantissa stored.
	CURVES_INV_WIDTH_IMPLICIT_BIT = 46,
	CURVES_INV_WIDTH_STORAGE_BITS = 46,
	CURVES_INV_WIDTH_STORAGE_MASK =
		(1ULL << CURVES_INV_WIDTH_STORAGE_BITS) - 1,

	// Shift field encoding: 6 bits, unsigned.
	CURVES_SHIFT_BITS = 6,
	CURVES_SHIFT_MASK = (1 << CURVES_SHIFT_BITS) - 1,
	CURVES_DENORMAL_SHIFT = 63
};

/**
 * struct curves_packed_segment - Cubic Hermite segment packed into 32 bytes.
 * @v: Array of 4 words containing packed data.
 *
 * Packs 5 normalized fixed-point values and their shifts into exactly half
 * of a 64-byte cache line.
 *
 * Values packed:
 *   - 4 polynomial coefficients (a, b signed; c, d unsigned)
 *   - 1 inverse segment width (unsigned)
 *   - 4 coefficient shifts (6-bit unsigned)
 *   - 1 inverse width shift (6-bit unsigned)
 *
 * Packing Layout (64 bits per word):
 *
 *      63                           19 18                                   0
 *      +-------------~  ~-------------+-------------------------------------+
 * v[0] |         coeff a (45)         |         inv_width[0..18] (19)       |
 *      +-------------~  ~-------------+-------------------------------------+
 * v[1] |         coeff b (45)         |         inv_width[19..37] (19)      |
 *      +-------------~  ~-------------+---------+-----------+---------------+
 * v[2] |         coeff c (45)         |iw[38-44]| iw_shift  |   shift_a     |
 *      |                              |  (7)    |   (6)     |     (6)       |
 *      +-------------~  ~-------------+---------+-----------+---------------+
 * v[3] |         coeff d (45)         |iw[45-45]| shift_d   | shift_c | shift_b |
 *      |                              |  (1)    |   (6)     |   (6)   |   (6)   |
 *      +-------------~  ~-------------+---------+-----------+---------+---------+
 *      63                           19 18      17          11         5         0
 *
 * Coefficient encoding:
 *   - Signed (a, b): sign-magnitude, implicit 1 at bit 44, 44-bit mantissa
 *   - Unsigned (c, d): implicit 1 at bit 45, 45-bit mantissa
 *
 * Inverse width encoding:
 *   - Unsigned, implicit 1 at bit 46, 46-bit mantissa scattered across words
 *
 * Shift encoding:
 *   - All shifts are 6-bit unsigned right-shift amounts
 *   - Value 63 indicates denormal (no implicit 1)
 */
struct curves_packed_segment {
	u64 v[CURVES_SEGMENT_COEFF_COUNT];
} __attribute__((aligned(32)));

/**
 * curves_unpack_segment() - Unpacks segment from wire format to math format.
 * @src: Pointer to packed segment data.
 *
 * Reconstructs coefficients with implicit leading 1 bits restored, converts
 * sign-magnitude to 2's complement for signed coefficients, and extracts
 * shift values.
 *
 * Return: Normalized segment ready for evaluation.
 */
static inline struct curves_normalized_segment
curves_unpack_segment(const struct curves_packed_segment *src)
{
	struct curves_normalized_segment dst;
	u64 payload[CURVES_SEGMENT_COEFF_COUNT];

	// Extract payloads (bottom 19 bits of each word).
	for (int i = 0; i < CURVES_SEGMENT_COEFF_COUNT; ++i) {
		payload[i] = src->v[i] &
			     ((1ULL << CURVES_SEGMENT_COEFF_SHIFT) - 1);
	}

	// Reconstruct inverse width from scattered bits.
	dst.inv_width.value = payload[0] | (payload[1] << 19) |
			      ((payload[2] >> 12) << 38) |
			      ((payload[3] >> 18) << 45);
	dst.inv_width.value |= (1ULL << CURVES_INV_WIDTH_IMPLICIT_BIT);

	// Extract shifts.
	dst.poly.shifts[0] = payload[2] & CURVES_SHIFT_MASK;
	dst.poly.shifts[1] = payload[3] & CURVES_SHIFT_MASK;
	dst.poly.shifts[2] = (payload[3] >> 6) & CURVES_SHIFT_MASK;
	dst.poly.shifts[3] = (payload[3] >> 12) & CURVES_SHIFT_MASK;
	dst.inv_width.shift = (payload[2] >> 6) & CURVES_SHIFT_MASK;

	// Unpack signed coefficients, (a, b).
	for (int i = 0; i < 2; ++i) {
		u64 raw = src->v[i] >> CURVES_SEGMENT_COEFF_SHIFT;
		u8 raw_shift = dst.poly.shifts[i];

		// Extract sign and mantissa.
		u64 sign_bit = raw >> CURVES_COEFF_SIGN_BIT;
		u64 mantissa = raw &
			       ((1ULL << CURVES_COEFF_SIGNED_IMPLICIT_BIT) - 1);

		// Detect denormal.
		u8 is_denorm = (raw_shift + 1) >> 6;
		dst.poly.shifts[i] = raw_shift - is_denorm;

		// Restore implicit 1 if not denormal.
		u64 implicit = (is_denorm ^ 1ULL)
			       << CURVES_COEFF_SIGNED_IMPLICIT_BIT;
		u64 abs_val = mantissa | implicit;

		// Convert sign-magnitude to 2's complement.
		dst.poly.coeffs[i] =
				(s64)((abs_val ^ -(s64)sign_bit) + sign_bit);
	}

	// Unpack unsigned coefficients, (c, d).
	for (int i = 2; i < CURVES_SEGMENT_COEFF_COUNT; ++i) {
		u64 raw = src->v[i] >> CURVES_SEGMENT_COEFF_SHIFT;
		u8 raw_shift = dst.poly.shifts[i];

		// Mantissa is full 45 bits (no sign bit).
		u64 mantissa = raw;

		// Detect denormal.
		u8 is_denorm = (raw_shift + 1) >> 6;
		dst.poly.shifts[i] = raw_shift - is_denorm;

		// Restore implicit 1 if not denormal.
		u64 implicit = (is_denorm ^ 1ULL)
			       << CURVES_COEFF_UNSIGNED_IMPLICIT_BIT;

		dst.poly.coeffs[i] = (s64)(mantissa | implicit);
	}

	return dst;
}

#endif /* _CURVES_SEGMENT_UNPACKING_H */
