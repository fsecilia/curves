// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Support for widening multiplication and narrowing division for 64-bit types.
 *
 * Most platforms handle 64-bit multiply and divide instructions with 128-bit
 * intermediates using register pairs, such as 64*64 -> 64:64, or 64:64/64 ->
 * 64. Accessing them from C usually means casting to __int128, but this can be
 * difficult for division; C promotes the denominator, and 128/128 divides are
 * not supported everywhere, absent most notably on x64.
 *
 * This module presents a uniform, 64-bit API to access these instructions from
 * kernel C in a form suitable for fixed point integers.
 *
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#ifndef _CURVES_MATH64_H
#define _CURVES_MATH64_H

#include "types.h"

/*
 * Kernel mode x64 fails to link if you try to invoke a 128/128 divide, so
 * we inline divq directly to invoke the 128/64 version.
 */
#if defined __KERNEL__ && defined __x86_64__

/*
 * __div_s128_by_s64 - Divide int128_t by int64_t
 *
 * Caller must ensure denominator != 0
 */
static uint64_t __div_s128_by_s64(int128_t numerator, int64_t denominator)
{
	int64_t numerator_high = numerator >> 64;
	int64_t numerator_low = numerator & 0xFFFFFFFFFFFFFFFF;
	int64_t quotient;
	asm("idivq %[denominator]"
	    : "=a"(quotient)
	    : "a"(numerator_low),
	      "d"(numerator_high), [denominator] "rm"(denominator)
	    : "cc", "rdx");
	return quotient;
}

#else

/*
 * In the general case, we just use 128-bit division.
*/
static inline int64_t __div_s128_by_s64(int128_t numerator, int64_t denominator)
{
	return numerator / denominator;
}

#endif

static inline int64_t curves_mul_i64_i64_shr(int64_t left, int64_t right,
					     unsigned shift)
{
	return (int64_t)(((int128_t)left * right) >> shift);
}

static inline int64_t
curves_div_i64_i64_shl(int64_t numerator, int64_t denominator, unsigned shift)
{
	return (int64_t)__div_s128_by_s64((int128_t)numerator << shift,
					  denominator);
}

#endif /* _CURVES_MATH64_H */
