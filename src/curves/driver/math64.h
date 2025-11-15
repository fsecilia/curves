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

#if !(defined __SIZEOF_INT128__ && __SIZEOF_INT128__ == 16)
#error This module requires 128-bit integer types, but they are not available.
#endif

__extension__ typedef __int128 int128_t;
__extension__ typedef unsigned __int128 uint128_t;

__extension__ typedef __int128 s128;
__extension__ typedef unsigned __int128 u128;

static inline s64 curves_saturate_s64(bool result_positive)
{
	return result_positive ? S64_MAX : S64_MIN;
}

// Truncates, rounding towards zero.
static inline int64_t curves_truncate_s64(unsigned int frac_bits, int64_t value,
					  unsigned int shift)
{
	// Extract sign of value: 0 if positive, -1 (all bits set) if negative.
	int64_t sign_mask = value >> 63;

	// Bias is (2^frac_bits - 1) for negatives, 0 for positives.
	int64_t bias = ((1LL << frac_bits) - 1) & sign_mask;

	// This can't overflow because it is only nonzero for negative numbers,
	// and it is never large enough to reach the end of the range.
	int64_t biased_value = value + bias;

	return biased_value >> shift;
}

static inline int64_t __curves_rescale_error_s64(int64_t value, int shift)
{
	// If the value is 0 or shift would underflow, return 0.
	if (value == 0 || shift < 0)
		return 0;

	// This would overflow. Saturate based on sign of product.
	return curves_saturate_s64(value >= 0);
}

// Shifts binary point to output_frac bits, truncating if necessary.
static inline int64_t curves_rescale_s64(unsigned int frac_bits, int64_t value,
					 unsigned int output_frac_bits)
{
	// Calculate final shift to align binary point with output_frac_bits.
	int shift = (int)output_frac_bits - (int)frac_bits;

	// Handle UB shifts.
	if (unlikely(shift >= 64 || shift <= -64))
		return __curves_rescale_error_s64(value, shift);

	// Shift into final place.
	if (output_frac_bits > frac_bits)
		return value << shift;
	else
		return curves_truncate_s64(frac_bits, value, -shift);
}

// Truncates, rounding toward zero.
static inline int64_t curves_truncate_s128(unsigned int frac_bits,
					   int128_t value, unsigned int shift)
{
	// Extract sign of value: 0 if positive, -1 (all bits set) if
	// negative.
	int128_t sign_mask = value >> 127;

	// Bias is (2^frac_bits - 1) for negatives, 0 for positives.
	int128_t bias = ((1LL << frac_bits) - 1) & sign_mask;

	// This can't overflow because it is only nonzero for negative numbers,
	// and it is never large enough to reach the end of the range.
	int128_t biased_value = value + bias;

	int128_t result = biased_value >> shift;

	if (unlikely(result != (int64_t)result)) {
		return curves_saturate_s64(result > 0);
	}

	return (int64_t)result;
}

// Shifts binary point to output_frac bits, truncating if necessary.
static inline int64_t __curves_rescale_error_s128(int128_t value, int shift)
{
	// If the value is 0 or shift would underflow, return 0.
	if (value == 0 || shift < 0)
		return 0;

	// This would overflow. Saturate based on sign of product.
	return curves_saturate_s64(value >= 0);
}

static inline int64_t curves_rescale_s128(unsigned int frac_bits,
					  int128_t value,
					  unsigned int output_frac_bits)
{
	// Calculate final shift to align binary point with output_frac_bits.
	int shift = (int)output_frac_bits - (int)frac_bits;

	// Handle UB shifts.
	if (unlikely(shift >= 128 || shift <= -128))
		return __curves_rescale_error_s128(value, shift);

	// Shift into final place.
	if (output_frac_bits > frac_bits)
		return value << shift;
	else
		return curves_truncate_s128(frac_bits, value, -shift);
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
 * Context: Undefined behavior if divisor is zero or quotient overflows int64_t.
 *          Traps with #DE on x64.
 * Return: 64-bit signed quotient
 */

#if defined __x86_64__

// x64: Use idivq directly to avoid missing 128/128 division instruction.
static inline int64_t curves_div_s128_s64(int128_t dividend, int64_t divisor)
{
	int64_t dividend_high = (int64_t)(dividend >> 64); // RDX
	int64_t dividend_low = (int64_t)dividend; // RAX
	int64_t quotient;

	asm("idivq %[divisor]"
	    : "=a"(quotient), "+d"(dividend_high)
	    : "a"(dividend_low), [divisor] "rm"(divisor)
	    : "cc");

	return quotient;
}

#else

// Generic case: Use compiler's existing 128-bit division operator.
static inline int64_t curves_div_s128_s64(int128_t dividend, int64_t divisor)
{
	return (int64_t)(dividend / divisor);
}

#endif

#endif /* _CURVES_MATH64_H */
