// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * 128-bit integer support.
 *
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#ifndef _CURVES_INT128_H
#define _CURVES_INT128_H

#include "driver.h"

#if defined __SIZEOF_INT128__ && __SIZEOF_INT128__ == 16

__extension__ typedef __int128 int128; //!< Suppress gcc warning.

#else

#if defined !__KERNEL__
#error 128-bit multiplication not supported on this platform.
#endif

#endif

#if defined __SIZEOF_INT128__ && __SIZEOF_INT128__ == 16

inline int64_t curves_mul_i64_i64_shr(int64_t left, int64_t right,
				      unsigned shift)
{
	return (int64_t)((int128)left * right >> shift);
}

#else

inline int64_t curves_mul_i64_i64_shr(int64_t left, int64_t right,
				      unsigned shift)
{
	uint64_t result = mul_u64_u64_shr(abs(left), abs(right), shift);
	return (left < 0) == (right < 0) ? result : -(int64_t)result;
}

#endif

#if defined __KERNEL__

inline int64_t curves_div_i64_i64_shl(int64_t numerator, int64_t denominator,
				      unsigned shift)
{
	return (int64_t)(div_s64((__int128)numerator << shift, denominator));
}

#else

inline int64_t curves_div_i64_i64_shl(int64_t numerator, int64_t denominator,
				      unsigned shift)
{
	return (int64_t)(((int128)numerator << shift) / denominator);
}

#endif

#endif /* _CURVES_INT128_H */
