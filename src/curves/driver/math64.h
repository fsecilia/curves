// SPDX-License-Identifier: GPL-2.0+ OR MIT
/**
 * Widening multiplication and narrowing division for 64-bit fixed-point math.
 *
 * Most platforms handle 64-bit multiply and divide instructions with 128-bit
 * intermediates using register pairs (e.g., 64*64 -> 64:64, or 64:64/64 -> 64).
 * Accessing them from C typically requires casting to __int128, but division
 * is problematic: C promotes the denominator, and 128/128 division is not
 * supported everywhere, absent most notably on x64.
 *
 * This module provides a uniform 64-bit API for these operations, suitable
 * for fixed-point arithmetic.
 *
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#ifndef _CURVES_MATH64_H
#define _CURVES_MATH64_H

#include "types.h"

/**
 * __div_s128_by_s64() - Divide 128-bit signed integer by 64-bit signed integer
 * @dividend: 128-bit value to divide
 * @divisor: 64-bit amount to divide by (must be non-zero)
 *
 * Performs 128/64 signed division. Caller must ensure divisor is non-zero.
 *
 * Return: 64-bit signed quotient
 */
#if defined __KERNEL__ && defined __x86_64__

// x64: Use idivq directly to avoid missing 128/128 division instruction.
static inline int64_t __div_s128_by_s64(int128_t dividend, int64_t divisor)
{
	int64_t dividend_high = dividend >> 64;
	int64_t dividend_low = dividend & 0xFFFFFFFFFFFFFFFF;
	int64_t quotient;

	asm("idivq %[divisor]"
	    : "=a"(quotient)
	    : "a"(dividend_low), "d"(dividend_high), [divisor] "rm"(divisor)
	    : "cc", "rdx");
	return quotient;
}

#else

// Generic case: Use compiler's existing 128-bit division operator.
static inline int64_t __div_s128_by_s64(int128_t numerator, int64_t denominator)
{
	return numerator / denominator;
}

#endif

/**
 * curves_mul_i64_i64_shr() - Widening multiply with right shift
 * @multiplicand: Value to multiply
 * @multiplier: Amount to multiply by
 * @shift: Number of bits to right-shift the product (must be < 64)
 *
 * Performs (multiplicand * multiplier) >> shift using 128-bit intermediate.
 * Returns 0 if shift >= 64.
 *
 * Return: 64-bit signed result
 */
static inline int64_t curves_mul_i64_i64_shr(int64_t multiplicand,
					     int64_t multiplier,
					     unsigned int shift)
{
	if (shift >= 64)
		return 0;
	return (int64_t)(((int128_t)multiplicand * multiplier) >> shift);
}

/**
 * curves_div_i64_i64_shl() - Left shift with narrowing divide
 * @dividend: Value to divide
 * @divisor: Amount to divide by (must be non-zero)
 * @shift: Number of bits to left-shift numerator before division (must be < 64)
 *
 * Performs (numerator << shift) / denominator using 128-bit intermediate.
 * Returns 0 if denominator is zero or shift >= 64.
 *
 * Return: 64-bit signed quotient
 */
static inline int64_t curves_div_i64_i64_shl(int64_t dividend, int64_t divisor,
					     unsigned int shift)
{
	if (divisor == 0)
		return 0;
	if (shift >= 64)
		return 0;
	return (int64_t)__div_s128_by_s64((int128_t)dividend << shift, divisor);
}

#endif /* _CURVES_MATH64_H */
