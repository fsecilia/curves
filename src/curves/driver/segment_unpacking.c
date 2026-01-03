// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Cubic hermite spline segment unpacking.
 *
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#include "segment_unpacking.h"

// Masks whole portion below coefficient.
static const u64 CURVES_SEGMENT_PAYLOAD_MASK =
	(1ULL << CURVES_SEGMENT_PAYLOAD_BITS) - 1;

// Masks individual payload fields.
static const u64 CURVES_SEGMENT_PAYLOAD_FIELD_MASK =
	(1ULL << CURVES_SEGMENT_PAYLOAD_FIELD_BITS) - 1;

// Converts an unsigned value to signed by shifting its sign bit into an s8
// msb, then arithmetic shifting back.
static inline s8 sign_extend(u8 value, unsigned int shift_msb)
{
	return (s8)(value << shift_msb) >> shift_msb;
}

// Extracts coefficient from top of packed element using an arithmetic shift to
// right align the contents.
static inline s64 extract_coeff(u64 packed)
{
	return (s64)packed >> CURVES_SEGMENT_COEFFICIENT_SHIFT;
}

// Extracts payload from bottom of packed element.
static inline u64 extract_payload(u64 packed)
{
	return packed & CURVES_SEGMENT_PAYLOAD_MASK;
}

// Extracts the top 7-bit field from the payload area of a packed element.
static inline u64 extract_payload_top(u64 packed)
{
	return extract_payload(packed) >> 2 * CURVES_SEGMENT_PAYLOAD_FIELD_BITS;
}

// Extracts a 6-bit field from the payload area of a packed element.
static inline u64 extract_payload_field(u64 packed, unsigned int index)
{
	return (packed >> index * CURVES_SEGMENT_PAYLOAD_FIELD_BITS) &
	       CURVES_SEGMENT_PAYLOAD_FIELD_MASK;
}

// Extracts a 6-bit signed shift.
static inline s8 extract_signed_payload_field(u64 packed, unsigned int index)
{
	const unsigned int shift_msb =
		BITS_PER_BYTE - CURVES_SEGMENT_PAYLOAD_FIELD_BITS;
	return sign_extend(extract_payload_field(packed, index), shift_msb);
}

struct curves_normalized_segment
curves_unpack_segment(const struct curves_packed_segment *src)
{
	struct curves_normalized_segment dst;

	// Coefficients.
	for (int i = 0; i < 4; ++i)
		dst.poly.coeffs[i] = extract_coeff(src->v[i]);

	// Gather inverse width.
	dst.inv_width.value = extract_payload(src->v[0]) |
			      extract_payload(src->v[1])
				      << CURVES_SEGMENT_PAYLOAD_BITS |
			      extract_payload_top(src->v[2])
				      << (2 * CURVES_SEGMENT_PAYLOAD_BITS);

	// Shifts.
	dst.poly.relative_shifts[0] =
		extract_signed_payload_field(src->v[2], 0);
	dst.poly.relative_shifts[1] =
		extract_signed_payload_field(src->v[3], 0);
	dst.poly.relative_shifts[2] =
		extract_signed_payload_field(src->v[3], 1);
	dst.poly.relative_shifts[3] =
		sign_extend(extract_payload_top(src->v[3]), 1);

	dst.inv_width.shift = extract_payload_field(src->v[2], 1);

	return dst;
}
