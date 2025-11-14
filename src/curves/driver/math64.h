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

static inline s64 curves_s64_saturate(bool result_positive)
{
	return result_positive ? S64_MAX : S64_MIN;
}

static inline s64 curves_s128_to_s64_truncate(s128 value)
{
	// Round trip should produce the same value.
	if (unlikely(value != (s64)value))
		return curves_s64_saturate(value >= 0);

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
 * Context: Undefined behavior if divisor is zero or quotient overflows int64_t.
 *          Traps with #DE on x64.
 * Return: 64-bit signed quotient
 */

struct curves_div_result {
	int64_t quotient;
	int64_t remainder;
};

#if defined __x86_64__

// x64: Use idivq directly to avoid missing 128/128 division instruction.
static inline struct curves_div_result curves_div_s128_by_s64(int128_t dividend,
							      int64_t divisor)
{
	int64_t remainder;
	int64_t quotient;

	asm("idivq %[divisor]"
	    : "=a"(quotient), "=d"(remainder)
	    : "a"((int64_t)dividend),
	      "d"((int64_t)(dividend >> 64)), [divisor] "rm"(divisor)
	    : "cc");

	return (struct curves_div_result){
		.quotient = quotient,
		.remainder = remainder,
	};
}

#else

// Generic case: Use compiler's existing 128-bit division operator.
static inline struct curves_div_result curves_div_s128_by_s64(int128_t dividend,
							      int64_t divisor)
{
	int64_t quotient = (int64_t)(dividend / divisor);
	int64_t remainder = (int64_t)(dividend - (int128_t)quotient * divisor);
	return (struct curves_div_result){
		.quotient = quotient,
		.remainder = remainder,
	};
}

#endif

// rounds to nearest
static inline s64 curves_div_s128_by_s64_rtn(int128_t dividend, s64 divisor)
{
	struct curves_div_result raw =
		curves_div_s128_by_s64(dividend, divisor);

	// Determine if rounding is necessary.
	s64 rem_sign = raw.remainder >> 63;
	u64 rem_abs = (u64)((raw.remainder ^ rem_sign) - rem_sign);
	s64 div_sign = divisor >> 63;
	u64 div_abs = (u64)((divisor ^ div_sign) - div_sign);
	bool needs_round = rem_abs >= (div_abs - rem_abs);

	// Determine rounding directions.
	bool is_negative = (dividend < 0) ^ (divisor < 0);
	bool round_up = needs_round && !is_negative;
	bool round_down = needs_round && is_negative;

	// Round with boundary saturation.
	return raw.quotient + (round_up && (raw.quotient != S64_MAX)) -
	       (round_down && (raw.quotient != S64_MIN));
}

#endif /* _CURVES_MATH64_H */
