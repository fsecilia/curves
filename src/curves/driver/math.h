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

// Saturates based on sign.
static inline s64 curves_saturate_s64(bool positive)
{
	return positive ? S64_MAX : S64_MIN;
}

// Saturates based on sign.
static inline s128 curves_saturate_s128(bool positive)
{
	return positive ? CURVES_S128_MAX : CURVES_S128_MIN;
}

/*
 * Generates a sign mask of 0 or -1.
 *
 * This copies the sign bit over all lower bits.
 */
static inline s64 curves_sign_mask(s64 value)
{
	return value >> 63;
}

// Convert signed -> unsigned magnitude.
static inline u64 curves_strip_sign(s64 value)
{
	s64 mask = curves_sign_mask(value);
	return (u64)((value + mask) ^ mask);
}

// Convert unsigned magnitude -> signed.
static inline s64 curves_apply_sign(u64 value, s64 mask)
{
	return (s64)((value + (u64)mask) ^ (u64)mask);
}

/*
 * Purely arithmetic max() for small integers.
 */
static inline u32 curves_max_u32(u32 a, u32 b)
{
	s64 delta = (s64)a - (s64)b;
	s64 mask = delta >> 63;
	return a - (u32)(delta & mask);
}

/*
 * Purely arithmetic min() for small integers.
 */
static inline u32 curves_min_u32(u32 a, u32 b)
{
	s64 delta = (s64)a - (s64)b;
	s64 mask = delta >> 63;
	return a - (u32)(delta & ~mask);
}

// Narrows a 128-bit value to 64 bits, saturating on overflow.
static inline s64 curves_narrow_s128_s64(s128 value)
{
	if (unlikely(value > (s128)S64_MAX))
		return S64_MAX;

	if (unlikely(value < (s128)S64_MIN))
		return S64_MIN;

	return (s64)value;
}

// Narrows a 128-bit value to 64 bits, saturating on overflow.
static inline u64 curves_narrow_u128_u64(u128 value)
{
	if (unlikely(value > (u128)U64_MAX))
		return U64_MAX;

	return (u64)value;
}

static inline s64 curves_add_saturate(s64 augend, s64 addend)
{
	s64 result;
	if (check_add_overflow(augend, addend, &result)) {
		return (augend > 0) ? S64_MAX : S64_MIN;
	}
	return result;
}

static inline s64 curves_subtract_saturate(s64 minuend, s64 subtrahend)
{
	s64 result;
	if (check_sub_overflow(minuend, subtrahend, &result)) {
		return (minuend > 0) ? S64_MAX : S64_MIN;
	}
	return result;
}

/**
 * curves_div_u128_by_u64() - Divide 128-bit signed integer by 64-bit signed
 * integer
 * @dividend: 128-bit value to divide
 * @divisor: 64-bit amount to divide by
 *
 * Performs 128/64 signed division. Caller must ensure divisor is non-zero and
 * that the quotient fits in a signed 64-bit integer.
 *
 * Context: Undefined behavior if divisor is zero or quotient overflows u64.
 *          Traps with #DE on x64.
 * Return: 64-bit signed quotient
 */

struct div_u128_u64_result {
	u64 quotient;
	u64 remainder;
};

#if defined __x86_64__

// x64: Use divq directly to avoid missing 128/128 division instruction.
static inline struct div_u128_u64_result curves_div_u128_u64(u128 dividend,
							     u64 divisor)
{
	struct div_u128_u64_result result;

	u64 high = (u64)(dividend >> 64);
	u64 low = (u64)dividend;

	asm volatile("divq %[divisor]"
		     : "=a"(result.quotient), "=d"(result.remainder)
		     : "d"(high), "a"(low), [divisor] "rm"(divisor)
		     : "cc");

	return result;
}

#else

// Generic case: Use compiler's existing 128-bit division operator.
static inline struct div_u128_u64_result curves_div_u128_u64(u128 dividend,
							     u64 divisor)
{
	struct div_u128_u64_result result = {
		.quotient = (u64)(dividend / divisor),
		.remainder = (u64)(dividend % divisor)
	};
	return result;
}

#endif

// Integer log2; undefined for value = 0
static inline u64 curves_log2_u64(u64 value)
{
	return 63 - curves_clz64((u64)value);
}

#endif /* _CURVES_MATH64_H */
