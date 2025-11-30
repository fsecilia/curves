// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Fixed-point number type and supporting math functions.
 *
 * This module uses arbitrary precision and does not have a particular
 * preferred Q format. Each operation taking a fixed point input also takes the
 * precision describing that input.
 *
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#ifndef _CURVES_FIXED_H
#define _CURVES_FIXED_H

#include "kernel_compat.h"
#include "math.h"

// ----------------------------------------------------------------------------
// s64 rescaler
// ----------------------------------------------------------------------------

s64 __cold __curves_fixed_rescale_error_s64(s64 value, unsigned int frac_bits,
					    unsigned int output_frac_bits);

// Shifts right, rounding towards nearest even (rne).
// Preconditions:
//   - shift must be in range [1, 63]
//   - caller is responsible for validating shift ranges
static inline s64 __curves_fixed_shr_rne_s64(s64 value, unsigned int shift)
{
	u64 half = 1ULL << (shift - 1);
	u64 frac_mask = (1ULL << shift) - 1;

	s64 int_part = value >> shift;
	u64 frac_part = (u64)value & frac_mask;

	s64 is_odd = int_part & 1;

	u64 bias = (half - 1) + (u64)is_odd;
	s64 carry = (s64)((frac_part + bias) >> shift);

	return int_part + carry;
}

// Shifts left, saturating if the value overflows.
// Preconditions:
//   - shift must be in range [0, 63]
//   - caller is responsible for validating shift range
static inline s64 __curves_fixed_shl_sat_s64(s64 value, unsigned int shift)
{
	s64 max_safe_val;
	s64 min_safe_val;

	// Find the maximum value that doesn't overflow.
	max_safe_val = S64_MAX >> shift;
	if (unlikely(value > max_safe_val))
		return S64_MAX;

	// Find the minimum value that doesn't overflow.
	min_safe_val = S64_MIN >> shift;
	if (unlikely(value < min_safe_val))
		return S64_MIN;

	// The value is safe to shift.
	return value << shift;
}

// Shifts binary point from frac_bits to output_frac_bits, truncating or
// saturating as necessary.
static inline s64 curves_fixed_rescale_s64(s64 value, unsigned int frac_bits,
					   unsigned int output_frac_bits)
{
	// Handle invalid scales
	if (unlikely(frac_bits >= 64 || output_frac_bits >= 64))
		return __curves_fixed_rescale_error_s64(value, frac_bits,
							output_frac_bits);

	// Shift into final place.
	if (output_frac_bits < frac_bits)
		return __curves_fixed_shr_rne_s64(value,
						  frac_bits - output_frac_bits);
	else
		return __curves_fixed_shl_sat_s64(value,
						  output_frac_bits - frac_bits);
}

// ----------------------------------------------------------------------------
// u64 rescaler
// ----------------------------------------------------------------------------

u64 __cold __curves_fixed_rescale_error_u64(u64 value, unsigned int frac_bits,
					    unsigned int output_frac_bits);

// Shifts right, rounding towards nearest even (rne).
// Preconditions:
//   - shift must be in range [1, 63]
//   - caller is responsible for validating shift ranges
static inline u64 __curves_fixed_shr_rne_u64(u64 value, unsigned int shift)
{
	u64 half = 1ULL << (shift - 1);
	u64 frac_mask = (1ULL << shift) - 1;

	u64 int_part = value >> shift;
	u64 frac_part = (u64)value & frac_mask;

	u64 is_odd = int_part & 1;

	u64 bias = (half - 1) + (u64)is_odd;
	u64 carry = (u64)((frac_part + bias) >> shift);

	return int_part + carry;
}

// Shifts left, saturating if the value overflows.
// Preconditions:
//   - shift must be in range [0, 64]
//   - caller is responsible for validating shift range
static inline u64 __curves_fixed_shl_sat_u64(u64 value, unsigned int shift)
{
	u64 max_safe_val;

	// Find the maximum value that doesn't overflow.
	max_safe_val = U64_MAX >> shift;
	if (unlikely(value > max_safe_val))
		return U64_MAX;

	// The value is safe to shift.
	return value << shift;
}

// Shifts binary point from frac_bits to output_frac_bits, truncating or
// saturating as necessary.
static inline u64 curves_fixed_rescale_u64(u64 value, unsigned int frac_bits,
					   unsigned int output_frac_bits)
{
	// Handle invalid scales
	if (unlikely(frac_bits >= 64 || output_frac_bits >= 64))
		return __curves_fixed_rescale_error_u64(value, frac_bits,
							output_frac_bits);

	// Shift into final place.
	if (output_frac_bits < frac_bits)
		return __curves_fixed_shr_rne_u64(value,
						  frac_bits - output_frac_bits);
	else
		return __curves_fixed_shl_sat_u64(value,
						  output_frac_bits - frac_bits);
}

// ----------------------------------------------------------------------------
// s128 rescaler
// ----------------------------------------------------------------------------

s128 __cold __curves_fixed_rescale_error_s128(s128 value,
					      unsigned int frac_bits,
					      unsigned int output_frac_bits);

// Shifts right, rounding towards nearest even (rne).
// Preconditions:
//   - shift must be in range [1, 127]
//   - caller is responsible for validating shift ranges
static inline s128 __curves_fixed_shr_rne_s128(s128 value, unsigned int shift)
{
	u128 half = (u128)1 << (shift - 1);
	u128 frac_mask = ((u128)1 << shift) - 1;

	s128 int_part = value >> shift;
	u128 frac_part = (u128)value & frac_mask;

	s128 is_odd = int_part & 1;

	u128 bias = (half - 1) + (u128)is_odd;
	s128 carry = (s128)((frac_part + bias) >> shift);

	return int_part + carry;
}

// Shifts left, saturating if the value overflows.
static inline s128 __curves_fixed_shl_sat_s128(s128 value, unsigned int shift)
{
	s128 max_safe_val;
	s128 min_safe_val;

	// Find the maximum value that doesn't overflow.
	max_safe_val = CURVES_S128_MAX >> shift;
	if (unlikely(value > max_safe_val))
		return CURVES_S128_MAX;

	// Find the minimum value that doesn't overflow.
	min_safe_val = CURVES_S128_MIN >> shift;
	if (unlikely(value < min_safe_val))
		return CURVES_S128_MIN;

	// The value is safe to shift.
	return value << shift;
}

// Shifts binary point from frac_bits to output_frac_bits, truncating or
// saturating as necessary.
static inline s128 curves_fixed_rescale_s128(s128 value, unsigned int frac_bits,
					     unsigned int output_frac_bits)
{
	// Handle invalid scales.
	if (unlikely(frac_bits >= 128 || output_frac_bits >= 128))
		return __curves_fixed_rescale_error_s128(value, frac_bits,
							 output_frac_bits);

	if (output_frac_bits < frac_bits)
		return __curves_fixed_shr_rne_s128(
			value, frac_bits - output_frac_bits);
	else
		return __curves_fixed_shl_sat_s128(value, output_frac_bits -
								  frac_bits);
}

// ----------------------------------------------------------------------------
// u128 rescaler
// ----------------------------------------------------------------------------

u128 __cold __curves_fixed_rescale_error_u128(u128 value,
					      unsigned int frac_bits,
					      unsigned int output_frac_bits);

// Shifts right, rounding towards nearest even (rne).
// Preconditions:
//   - shift must be in range [1, 127]
//   - caller is responsible for validating shift ranges
static inline u128 __curves_fixed_shr_rne_u128(u128 value, unsigned int shift)
{
	u128 half = (u128)1 << (shift - 1);
	u128 frac_mask = ((u128)1 << shift) - 1;

	u128 int_part = value >> shift;
	u128 frac_part = value & frac_mask;

	u128 is_odd = int_part & 1;

	u128 bias = half - 1 + is_odd;
	u128 carry = (frac_part + bias) >> shift;

	return int_part + carry;
}

// Shifts left, saturating if the value overflows.
static inline u128 __curves_fixed_shl_sat_u128(u128 value, unsigned int shift)
{
	u128 max_safe_val;

	// Find the maximum value that doesn't overflow.
	max_safe_val = CURVES_U128_MAX >> shift;
	if (unlikely(value > max_safe_val))
		return CURVES_U128_MAX;

	// The value is safe to shift.
	return value << shift;
}

// Shifts binary point from frac_bits to output_frac_bits, truncating or
// saturating as necessary.
static inline u128 curves_fixed_rescale_u128(u128 value, unsigned int frac_bits,
					     unsigned int output_frac_bits)
{
	// Handle invalid scales.
	if (unlikely(frac_bits >= 128 || output_frac_bits >= 128))
		return __curves_fixed_rescale_error_u128(value, frac_bits,
							 output_frac_bits);

	if (output_frac_bits < frac_bits)
		return __curves_fixed_shr_rne_u128(
			value, frac_bits - output_frac_bits);
	else
		return __curves_fixed_shl_sat_u128(value, output_frac_bits -
								  frac_bits);
}

// ----------------------------------------------------------------------------
// Conversions
// ----------------------------------------------------------------------------

/**
 * curves_fixed_from_integer() - Converts integers to fixed point
 * @frac_bits: Fractional bit precision, [0, 62].
 * @value: Integer to convert
 *
 * Return: value in fixed-point with given precision.
 */
static inline s64 curves_fixed_from_integer(s64 value, unsigned int frac_bits)
{
	return curves_fixed_rescale_s64(value, 0, frac_bits);
}

/**
 * curves_fixed_to_integer() - Converts fixed point to integers
 * @frac_bits: Fractional bit precision, [0, 62].
 * @value: Fixed-point with given precision to convert
 *
 * Return: value truncated to an integer.
 */
static inline s64 curves_fixed_to_integer(s64 value, unsigned int frac_bits)
{
	return curves_fixed_rescale_s64(value, frac_bits, 0);
}

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

/**
 * curves_fixed_const_1() - Fixed-point constant 1.
 * @frac_bits: Fractional bit precision, [0 to 62].
 *
 * Return: Constant value 1 with specified precision.
 */
#define CURVES_FIXED_1 1
#define CURVES_FIXED_1_FRAC_BITS 62
static inline s64 curves_fixed_const_1(unsigned int frac_bits)
{
	return curves_fixed_from_integer(CURVES_FIXED_1, frac_bits);
}

/**
 * curves_fixed_const_1_5() - Fixed-point constant 1.5.
 * @frac_bits: Fractional bit precision, [0 to CURVES_1_5_FRAC_BITS].
 *
 * Return: Constant value e with specified precision.
 */
#define CURVES_FIXED_1_5 6917529027641081856LL
#define CURVES_FIXED_1_5_FRAC_BITS 62
static inline s64 curves_fixed_const_1_5(unsigned int frac_bits)
{
	// This value was generated using wolfram alpha: round(1.5*2^62)
	return curves_fixed_rescale_s64(CURVES_FIXED_1_5,
					CURVES_FIXED_1_5_FRAC_BITS, frac_bits);
}

/**
 * curves_fixed_const_e() - Fixed-point constant e.
 * @frac_bits: Fractional bit precision, [0 to CURVES_E_FRAC_BITS].
 *
 * Return: Constant value e with specified precision.
 */
#define CURVES_FIXED_E 6267931151224907085LL
#define CURVES_FIXED_E_FRAC_BITS 61
static inline s64 curves_fixed_const_e(unsigned int frac_bits)
{
	// This value was generated using wolfram alpha: round(e*2^61)
	return curves_fixed_rescale_s64(CURVES_FIXED_E,
					CURVES_FIXED_E_FRAC_BITS, frac_bits);
}

/**
 * curves_fixed_const_ln2() - Fixed-point constant ln(2).
 * @frac_bits: Fractional bit precision, [0 to CURVES_LN2_FRAC_BITS].
 *
 * Return: Constant value ln(2) with specified precision.
 */
#define CURVES_FIXED_LN2 3196577161300663915LL
#define CURVES_FIXED_LN2_FRAC_BITS 62
static inline s64 curves_fixed_const_ln2(unsigned int frac_bits)
{
	// This value was generated using wolfram alpha: round(log(2)*2^62)
	return curves_fixed_rescale_s64(CURVES_FIXED_LN2,
					CURVES_FIXED_LN2_FRAC_BITS, frac_bits);
}

/**
 * curves_fixed_const_pi() - Fixed-point constant pi.
 * @frac_bits: Fractional bit precision, [0 to CURVES_PI_FRAC_BITS].
 *
 * Return: Constant value pi with specified precision.
 */
#define CURVES_FIXED_PI 7244019458077122842LL
#define CURVES_FIXED_PI_FRAC_BITS 61
static inline s64 curves_fixed_const_pi(unsigned int frac_bits)
{
	// This value was generated using wolfram alpha: round(pi*2^61)
	return curves_fixed_rescale_s64(CURVES_FIXED_PI,
					CURVES_FIXED_PI_FRAC_BITS, frac_bits);
}

// ----------------------------------------------------------------------------
// Addition
// ----------------------------------------------------------------------------

s64 __cold __curves_fixed_add_error(unsigned int augend_frac_bits,
				    unsigned int addend_frac_bits,
				    unsigned int output_frac_bits);

static inline s64 curves_fixed_add(s64 augend, unsigned int augend_frac_bits,
				   s64 addend, unsigned int addend_frac_bits,
				   unsigned int output_frac_bits)
{
	unsigned int max_frac_bits;
	s128 wide_augend, wide_addend;

	// Validate inputs.
	if (unlikely(augend_frac_bits >= 64 || addend_frac_bits >= 64 ||
		     output_frac_bits >= 64)) {
		return __curves_fixed_add_error(
			augend_frac_bits, addend_frac_bits, output_frac_bits);
	}

	// Use max of 3 to determine highest precision.
	max_frac_bits = curves_max_u32(augend_frac_bits, addend_frac_bits);
	max_frac_bits = curves_max_u32(max_frac_bits, output_frac_bits);

	// Promote and align.
	wide_augend = (s128)augend << (max_frac_bits - augend_frac_bits);
	wide_addend = (s128)addend << (max_frac_bits - addend_frac_bits);

	// Sum, rescale, and narrow.
	return curves_narrow_s128_s64(curves_fixed_rescale_s128(
		wide_augend + wide_addend, max_frac_bits, output_frac_bits));
}

// ----------------------------------------------------------------------------
// Subtraction
// ----------------------------------------------------------------------------

s64 __cold __curves_fixed_subtract_error(unsigned int minuend_frac_bits,
					 unsigned int subtrahend_frac_bits,
					 unsigned int output_frac_bits);

static inline s64 curves_fixed_subtract(s64 minuend,
					unsigned int minuend_frac_bits,
					s64 subtrahend,
					unsigned int subtrahend_frac_bits,
					unsigned int output_frac_bits)
{
	unsigned int max_frac_bits;
	s128 wide_minuend, wide_subtrahend;

	// Validate inputs.
	if (unlikely(minuend_frac_bits >= 64 || subtrahend_frac_bits >= 64 ||
		     output_frac_bits >= 64)) {
		return __curves_fixed_subtract_error(minuend_frac_bits,
						     subtrahend_frac_bits,
						     output_frac_bits);
	}

	// Use max of 3 to determine highest precision.
	max_frac_bits = curves_max_u32(minuend_frac_bits, subtrahend_frac_bits);
	max_frac_bits = curves_max_u32(max_frac_bits, output_frac_bits);

	// Promote and align.
	wide_minuend = (s128)minuend << (max_frac_bits - minuend_frac_bits);
	wide_subtrahend = (s128)subtrahend
			  << (max_frac_bits - subtrahend_frac_bits);

	// Sum, rescale, and narrow.
	return curves_narrow_s128_s64(
		curves_fixed_rescale_s128(wide_minuend - wide_subtrahend,
					  max_frac_bits, output_frac_bits));
}

// ----------------------------------------------------------------------------
// Multiplication
// ----------------------------------------------------------------------------

/**
 * curves_fixed_multiply() - Multiplies two variable-precision, fixed-point
 * values.
 *
 * @multiplicand_frac_bits: Fractional bits in multiplicand, [0, 62].
 * @multiplicand: Value to multiply.
 * @multiplier_frac_bits: Fractional bits in multiplier, [0, 62].
 * @multiplier: Amount to multiply by.
 * @output_frac_bits: Fractional bits in result, [0, 62].
 *
 * This function multiplies two fixed-point values with independent precisions
 * and shifts the result to match @output_frac_bits. The raw product has
 * @multiplicand_frac_bits + @multiplier_frac_bits fractional bits; this
 * function shifts it left or right as needed to produce the requested output
 * precision.
 *
 * The shift and multiply are done at 128 bits before rounding the result to
 * 64 bits. Rounding is always towards zero.
 *
 * Return: Product shifted to @output_frac_bits precision, 0 on underflow, or
 * signed saturation on overflow.
 */
static inline s64 curves_fixed_multiply(s64 multiplicand,
					unsigned int multiplicand_frac_bits,
					s64 multiplier,
					unsigned int multiplier_frac_bits,
					unsigned int output_frac_bits)
{
	return curves_narrow_s128_s64(curves_fixed_rescale_s128(
		(s128)multiplicand * (s128)multiplier,
		multiplicand_frac_bits + multiplier_frac_bits,
		output_frac_bits));
}

/*
 * Fused Multiply-Add: (multiplicand * multiplier) + addend
 * Performs the addition in full 128-bit precision before rounding.
 */
static inline s64
curves_fixed_fma(s64 multiplicand, unsigned int multiplicand_frac_bits,
		 s64 multiplier, unsigned int multiplier_frac_bits, s64 addend,
		 unsigned int addend_frac_bits, unsigned int output_frac_bits)
{
	u32 product_frac_bits = multiplicand_frac_bits + multiplier_frac_bits;
	s32 frac_bits_delta = (s32)product_frac_bits - (s32)addend_frac_bits;

	s32 sign_mask = frac_bits_delta >> 31;
	u32 addend_shift = (u32)(frac_bits_delta & ~sign_mask);
	u32 product_shift = (u32)(-frac_bits_delta & sign_mask);

	u32 max_frac_bits = product_frac_bits + product_shift;

	s128 product = ((s128)multiplicand * (s128)multiplier) << product_shift;
	s128 wide_addend = (s128)addend << addend_shift;

	return curves_narrow_s128_s64(curves_fixed_rescale_s128(
		product + wide_addend, max_frac_bits, output_frac_bits));
}

// ----------------------------------------------------------------------------
// Division
// ----------------------------------------------------------------------------

s64 __cold __curves_fixed_divide_error(s64 dividend, s64 divisor);

/**
 * Calculates the 128-bit left shift to apply to a 64-bit dividend that
 * maximizes precision without overflowing.
 *
 * To divide safely, the high 64 bits of the dividend must be strictly less
 * than the divisor. This function finds the largest 'shift' such that:
 *
 *	(dividend << shift) >> 64 < divisor
 *
 * This guarantees the resulting quotient fits in a u64 without trapping.
 *
 * Returns: maximum left shift that guarantees quotient fits into an u64.
 */
static inline int __curves_fixed_divide_optimal_shift(u64 dividend, u64 divisor)
{
	// Determine headroom.
	//
	// We use clz(dividend | 1) here because clz(0) is UB. This only
	// affects the dividend == 0 case, and that case does not affect
	// division. This saves checking dividend == 0 explicitly in a
	// conditional.
	int dividend_shift = (int)curves_clz64(dividend | 1);
	int divisor_shift = (int)curves_clz64(divisor);

	// Calculate conservative shift.
	//
	// This is the largest shift guaranteed not to overflow the division.
	// It gives up 1 bit of precision to ensure safety.
	int total_shift = 64 + dividend_shift - divisor_shift - 1;

	// Reclaim the lost bit if safe.
	//
	// We can shift one more bit when divisor is larger than dividend.
	// Normalize values to align MSBs, then their difference has the high
	// bit set when safe. Add that bit back to the total shift.
	u64 aligned_dividend = dividend << dividend_shift;
	u64 aligned_divisor = divisor << divisor_shift;
	total_shift += (aligned_dividend - aligned_divisor) >> 63;

	return total_shift;
}

/*
 * Round-Nearest-Even for right shifts.
 *
 * Shifts to align binary point then uses remainder to perform rne.
 *
 * Returns: quotient shifted right and rounded using rne, U64_MAX on overflow.
 */
static inline u64 __curves_fixed_divide_shr_rne(u64 quotient, u64 remainder,
						unsigned int shift)
{
	// Split quotient into int and frac parts.
	u64 frac_mask = (1ULL << shift) - 1;
	u64 frac_part = quotient & frac_mask;
	u64 int_part = quotient >> shift;

	// Decide if tie is possible and tiebreaker is necessary.
	u64 any_remainder = (remainder | -remainder) >> 63;
	u64 is_odd = int_part & 1;
	u64 tiebreaker = is_odd | any_remainder;

	// Apply bias to determine if we need a carry.
	u64 half = 1ULL << (shift - 1);
	u64 total_bias = (half - 1) + tiebreaker;
	u64 carry = (frac_part + total_bias) >> shift;

	// Apply carry.
	return int_part + carry;
}

/*
 * Round-Nearest-Even for exact alignment.
 *
 * This implementation starts with the standard rounding threshold,
 * floor(divisor / 2), then lowers it by 1 if we have a tiebreaker to force
 * a round-up on exact halves.
 *
 * Returns: quotient rounded using rne, U64_MAX on overflow.
 */
static inline u64 __curves_fixed_divide_rne_exact(u64 quotient, u64 remainder,
						  u64 divisor)
{
	// Decide if tiebreaker is required.
	//
	// A tie is only possible if the divisor is even (~divisor & 1), then a
	// tiebreaker is then only necessary if the quotient is odd
	// (quotient & 1).
	u64 is_tie = (~divisor & 1) & (quotient & 1);

	// Determine if carry is required.
	//
	// We need a carry if the remainder is larger than the threshold.
	u64 threshold = (divisor >> 1) - is_tie;
	u64 carry = (threshold - remainder) >> 63;

	// Check for saturation.
	//
	// If we are already at the limit and need to round up, saturate.
	if (unlikely(quotient == U64_MAX && carry))
		return U64_MAX;

	// Apply carry.
	return quotient + carry;
}

static inline u64 __curves_fixed_divide(u64 dividend,
					unsigned int dividend_frac_bits,
					u64 divisor,
					unsigned int divisor_frac_bits,
					unsigned int output_frac_bits)
{
	struct div_u128_u64_result div_res;

	// Determine shift budget.
	int final_shift = (int)output_frac_bits - (int)dividend_frac_bits +
			  (int)divisor_frac_bits;
	int initial_shift =
		__curves_fixed_divide_optimal_shift(dividend, divisor);
	int remaining_shift = final_shift - initial_shift;

	// Range check shifts.
	if (unlikely(remaining_shift > 0)) {
		// We already shift left as far as possible. Any further left
		// shift must overflow, so saturate.
		return U64_MAX;
	}
	if (unlikely(remaining_shift <= -64)) {
		// Right shifting all bits is always zero.
		return 0;
	}

	// Shift as far left as possible and divide.
	div_res = curves_div_u128_u64((u128)dividend << initial_shift, divisor);

	// Shift right what remains and apply rne.
	if (likely(remaining_shift < 0)) {
		return __curves_fixed_divide_shr_rne(
			div_res.quotient, div_res.remainder,
			(unsigned int)-remaining_shift);
	} else {
		// remaining_shift == 0
		return __curves_fixed_divide_rne_exact(
			div_res.quotient, div_res.remainder, divisor);
	}
}

static inline s64 curves_fixed_divide(s64 dividend,
				      unsigned int dividend_frac_bits,
				      s64 divisor,
				      unsigned int divisor_frac_bits,
				      unsigned int output_frac_bits)
{
	u64 range_max, quotient;
	s64 quotient_sign_mask;

	// Validate inputs.
	if (unlikely(dividend_frac_bits >= 64 || divisor_frac_bits >= 64 ||
		     output_frac_bits >= 64 || divisor == 0))
		return __curves_fixed_divide_error(dividend, divisor);

	// Forward to unsigned implementation.
	quotient_sign_mask = curves_sign_mask(dividend ^ divisor);
	quotient = __curves_fixed_divide(curves_strip_sign(dividend),
					 dividend_frac_bits,
					 curves_strip_sign(divisor),
					 divisor_frac_bits, output_frac_bits);

	// Range check before converting back to signed.
	//
	// range_max is S64_MAX when quotient >= 0, -S64_MIN otherwise.
	range_max = (u64)S64_MAX - (u64)quotient_sign_mask;
	if (unlikely(quotient > range_max))
		return quotient_sign_mask ? S64_MIN : S64_MAX;

	// Apply sign to quotient.
	return curves_apply_sign(quotient, quotient_sign_mask);
}

// ----------------------------------------------------------------------------
// Roots
// ----------------------------------------------------------------------------

/**
 * curves_fixed_isqrt() - Newton-Raphson solver for inverse sqrt.
 *
 * This function solves `y = 1/sqrt(x)` using Newton-Raphson. We define a
 * function, `f(y)`, with the same roots as y, start with an initial guess near
 * the solution, then iterate using the recurrence relation:
 *
 *     `y[n + 1] = y[n] - f(y[n])/f'(y[n])`
 *
 * Each step finds the line tangent to `f(y[n])`, finds the horizontal
 * intercept of that tangent, then repeats with `y[n + 1]` set to that
 * intercept. With a good initial guess for `y[0]`, this converges
 * quadratically to the root of `f(y)`.
 *
 * In the case of `y = 1/sqrt(x)`, we choose `f(y) = y^-2 - x`:
 *
 *   y = 1/sqrt(x)   // given
 *   y^2 = 1/x       // square both sides
 *   xy^2 = 1        // multiply both sides by x
 *   x = y^-2        // divide both sides by y^2
 *   0 = y^-2 - x    // find root
 *
 * There are other choices, but this has an important property. Given
 * `f'(y) = -2y^-3`:
 *
 *   y + f(y)/f'(y) = y - (y^-2 - x)/(-2y^-3)   // given
 *                  = y + y^3(y^-2 - x)/2       // move -y^3 to denominator
 *                  = y + y(1 - xy^2)/2         // distribute y^2
 *                  = y(1 + (1 - xy^2)/2)       // factor out common y
 *                  = y(3 - xy^2)/2             // combine constants
 *
 * This form allows calculating isqrt using only multiplication, subtraction,
 * and a shift.
 *
 * The initial guess is found using a quadratic approximation of `1/sqrt(x)`
 * using Horner's method: `C0 + -C1*x + C2*x^2 = C0 - x*(C1 - x*C2)`.
 *
 * Using a quadratic approximation balances Horner iterations against
 * Newton-Raphson iterations. Each NR iteration uses 3 multiplies. Horner
 * iterations use 1. For the same precision, a -log2/2 approximation requires 6
 * iterations. Linear requires 4. Quadratic requires 3. Cubic also requires 3,
 * so we use quadratic.
 *
 * Returns: inverse sqrt
 */
static inline u64 curves_fixed_isqrt(u64 x, unsigned int frac_bits,
				     unsigned int output_frac_bits)
{
	// Quadratic approximation coefficients.
	//
	// The constants are in Q2.62, so they're scaled by 2^62 and rounded.
	// See src/curves/tools/isqrt_initial_guess.sollya for more information
	// about how these values are generated.
	u64 C0_Q62 = 10354071711462988194ULL;
	u64 C1_Q62 = 9674659108971248202ULL;
	u64 C2_Q62 = 3949952137299739940ULL;

	unsigned int X_NORM_FRAC_BITS = 64;
	unsigned int Y_FRAC_BITS = 62;
	u64 THREE_Q62 = 3ULL << 62;
	u64 SQRT2_Q62 = 0x5A827999FCEF3242ULL;

	unsigned int x_lz, x_norm_exponent, y_denorm_frac_bits;
	u64 c1, c2, x_norm, y, yy, factor;

	if (unlikely(x == 0))
		return U64_MAX;

	// Normalize x to Q0.64 [0.5, 1.0).
	x_lz = curves_clz64(x);
	x_norm = x << x_lz;
	x_norm_exponent = x_lz + frac_bits;

	// Approximate 1/sqrt for initial guess using Horner's method.
	c2 = (u64)(((u128)x_norm * C2_Q62) >> X_NORM_FRAC_BITS);
	c1 = (u64)(((u128)x_norm * (C1_Q62 - c2)) >> X_NORM_FRAC_BITS);
	y = C0_Q62 - c1;

	// Newton-Raphson.
	for (int i = 0; i < 3; ++i) {
		yy = (u64)(((u128)y * y) >> Y_FRAC_BITS);
		factor = ((u128)x_norm * yy) >> X_NORM_FRAC_BITS;
		y = (u64)(((u128)y * (THREE_Q62 - factor)) >>
			  (Y_FRAC_BITS + 1));
	}

	// Denormalize.
	if (x_norm_exponent & 1)
		y = (u64)(((u128)y * SQRT2_Q62) >> Y_FRAC_BITS);
	y_denorm_frac_bits =
		Y_FRAC_BITS + (X_NORM_FRAC_BITS >> 1) - (x_norm_exponent >> 1);

	return curves_narrow_u128_u64(curves_fixed_rescale_u128(
		(u128)y, y_denorm_frac_bits, output_frac_bits));
}

static inline u64 curves_fixed_exp2(s64 x, unsigned int frac_bits,
				    unsigned int output_frac_bits)
{
	int POLY_DEGREE = 12;
	u64 COEFFS[] = {
		9223372036854775809ULL,	 12786308645202655203ULL,
		17725587574383041906ULL, 16381921401261540299ULL,
		11355082631786253471ULL, 12593189599434484850ULL,
		11638579126942535220ULL, 18439395033312365335ULL,
		12781686105372208973ULL, 15740085723622738413ULL,
		17608227533117276839ULL, 16306190281335833172ULL,
		11548001989246909982ULL,
	};
	unsigned int FRAC_BITS[] = {
		63, 64, 66, 68, 70, 73, 76, 80, 83, 87, 91, 95, 98,
	};

	u64 result;

	s64 int_part = x >> frac_bits;
	u64 frac_part_norm = (u64)x << (64 - frac_bits);

	u64 acc = COEFFS[POLY_DEGREE];

	for (int i = POLY_DEGREE; i > 0; --i) {
		u128 prod = (u128)acc * frac_part_norm;
		int shift = FRAC_BITS[i] - FRAC_BITS[i - 1];
		acc = (u64)(prod >> (64 + shift)) + COEFFS[i - 1];
	}

	result = curves_fixed_rescale_u64(acc, 64 - int_part, output_frac_bits);

	return result;
}

#endif /* _CURVES_FIXED_H */
