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

#include "kernel_compat.h"

#define CURVES_U128_MAX (~(u128)0)

#define CURVES_S128_MAX ((s128)(CURVES_U128_MAX >> 1))
#define CURVES_S128_MIN (-CURVES_S128_MAX - 1)

// Narrows a 128-bit value to 64-bits, saturating on overflow.
static inline s64 curves_narrow_s128_s64(s128 value)
{
	if (unlikely(value > (s128)S64_MAX))
		return S64_MAX;

	if (unlikely(value < (s128)S64_MIN))
		return S64_MIN;

	return (s64)value;
}

/**
 * curves_div_s128_by_s64() - Divide 128-bit signed integer by 64-bit signed
 * integer
 * @dividend: 128-bit value to divide
 * @divisor: 64-bit amount to divide by
 *
 * Performs 128/64 signed division. Caller must ensure divisor is non-zero and
 * that the quotient fits in a signed 64-bit integer.
 *
 * Common overflow cases:
 * - Magnitude of top 65 bits of dividend >= magnitude of divisor
 * - Dividend is S64_MIN and divisor is -1
 *
 * Context: Undefined behavior if divisor is zero or quotient overflows s64.
 *          Traps with #DE on x64.
 * Return: 64-bit signed quotient
 */

#if defined __x86_64__

// x64: Use idivq directly to avoid missing 128/128 division instruction.
static inline s64 curves_div_s128_s64(s128 dividend, s64 divisor)
{
	s64 dividend_high = (s64)(dividend >> 64); // RDX
	s64 dividend_low = (s64)dividend; // RAX
	s64 quotient;

	asm("idivq %[divisor]"
	    : "=a"(quotient), "+d"(dividend_high)
	    : "a"(dividend_low), [divisor] "rm"(divisor)
	    : "cc");

	return quotient;
}

#else

// Generic case: Use compiler's existing 128-bit division operator.
static inline s64 curves_div_s128_s64(int128_t dividend, s64 divisor)
{
	return (s64)(dividend / divisor);
}

#endif

#endif /* _CURVES_MATH64_H */
