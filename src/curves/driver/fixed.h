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
#include "math64.h"

// ----------------------------------------------------------------------------
// 64-bit API
// ----------------------------------------------------------------------------

// Saturates based on sign.
static inline s64 curves_saturate_s64(bool positive)
{
	return positive ? S64_MAX : S64_MIN;
}

s64 __cold __curves_fixed_rescale_error_s64(s64 value, unsigned int frac_bits,
					    unsigned int output_frac_bits);

// Shifts right, rounding towards zero.
// Preconditions:
//   - shift must be in range [0, 63]
//   - caller is responsible for validating shift range
static inline s64 __curves_fixed_shr_rtz_s64(s64 value, unsigned int shift)
{
	// To round up during division, bias dividend by divisor - 1.
	s64 divisor = 1LL << shift;
	s64 bias = divisor - 1;

	// Positive numbers already round towards zero.
	// Apply bias only when value is negative.
	s64 sign_mask = value >> 63;
	s64 biased_value = value + (bias & sign_mask);

	// Perform division.
	return biased_value >> shift;
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
		return __curves_fixed_shr_rtz_s64(value,
						  frac_bits - output_frac_bits);
	else
		return __curves_fixed_shl_sat_s64(value,
						  output_frac_bits - frac_bits);
}

// ----------------------------------------------------------------------------
// 128-bit API
// ----------------------------------------------------------------------------

// Saturates based on sign.
static inline s128 curves_saturate_s128(bool positive)
{
	return positive ? CURVES_S128_MAX : CURVES_S128_MIN;
}

s128 __cold __curves_fixed_rescale_error_s128(s128 value,
					      unsigned int frac_bits,
					      unsigned int output_frac_bits);

// Shifts right, rounding towards zero.
static inline s128 __curves_fixed_shr_rtz_s128(s128 value, unsigned int shift)
{
	// To round up during division, bias dividend by divisor - 1.
	s128 divisor = (s128)1 << shift;
	s128 bias = divisor - 1;

	// Positive numbers already round towards zero.
	// Apply bias only when value is negative.
	s128 sign_mask = value >> 127;
	s128 biased_value = value + (bias & sign_mask);

	// Perform division.
	return biased_value >> shift;
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

	// Shift into final place.
	if (output_frac_bits < frac_bits)
		return __curves_fixed_shr_rtz_s128(
			value, frac_bits - output_frac_bits);
	else
		return __curves_fixed_shl_sat_s128(value, output_frac_bits -
								  frac_bits);
}

// Narrows a 128-bit fixed-point value to 64-bits, saturating on overflow.
static inline s64 curves_fixed_narrow_s128_s64(s128 value)
{
	if (unlikely(value != (s64)value)) {
		return curves_saturate_s64(value > 0);
	}

	return (s64)value;
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
#define CURVES_FIXED_1_FRAC_BITS 62
static inline s64 curves_fixed_const_1(unsigned int frac_bits)
{
	return curves_fixed_from_integer(1, frac_bits);
}

/**
 * curves_fixed_const_e() - Fixed-point constant e.
 * @frac_bits: Fractional bit precision, [0 to CURVES_E_FRAC_BITS].
 *
 * Return: Constant value e with specified precision.
 */
#define CURVES_FIXED_E_FRAC_BITS 61
static inline s64 curves_fixed_const_e(unsigned int frac_bits)
{
	// This value was generated using wolfram alpha: round(e*2^61)
	return curves_fixed_rescale_s64(6267931151224907085ll,
					CURVES_FIXED_E_FRAC_BITS, frac_bits);
}

/**
 * curves_fixed_const_ln2() - Fixed-point constant ln(2).
 * @frac_bits: Fractional bit precision, [0 to CURVES_LN2_FRAC_BITS].
 *
 * Return: Constant value ln(2) with specified precision.
 */
#define CURVES_FIXED_LN2_FRAC_BITS 62
static inline s64 curves_fixed_const_ln2(unsigned int frac_bits)
{
	// This value was generated using wolfram alpha: round(log(2)*2^62)
	return curves_fixed_rescale_s64(3196577161300663915ll,
					CURVES_FIXED_LN2_FRAC_BITS, frac_bits);
}

/**
 * curves_fixed_const_pi() - Fixed-point constant pi.
 * @frac_bits: Fractional bit precision, [0 to CURVES_PI_FRAC_BITS].
 *
 * Return: Constant value pi with specified precision.
 */
#define CURVES_FIXED_PI_FRAC_BITS 61
static inline s64 curves_fixed_const_pi(unsigned int frac_bits)
{
	// This value was generated using wolfram alpha: round(pi*2^61)
	return curves_fixed_rescale_s64(7244019458077122842ll,
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
	s64 intermediate_augend, intermediate_addend;
	unsigned int max_frac_bits;

	// Validate inputs.
	if (unlikely(augend_frac_bits >= 64 || addend_frac_bits >= 64 ||
		     output_frac_bits >= 64)) {
		return __curves_fixed_add_error(
			augend_frac_bits, addend_frac_bits, output_frac_bits);
	}

	// Choose greater intermediate precision.
	max_frac_bits = output_frac_bits;
	if (max_frac_bits < augend_frac_bits)
		max_frac_bits = augend_frac_bits;
	if (max_frac_bits < addend_frac_bits)
		max_frac_bits = addend_frac_bits;

	// Promote both summands to greater intermediate precision.
	intermediate_augend = curves_fixed_rescale_s64(augend, augend_frac_bits,
						       max_frac_bits);
	intermediate_addend = curves_fixed_rescale_s64(addend, addend_frac_bits,
						       max_frac_bits);

	// Check for saturation.
	if (intermediate_addend > 0 &&
	    intermediate_augend > S64_MAX - intermediate_addend)
		return curves_saturate_s64(true);
	if (intermediate_addend < 0 &&
	    intermediate_augend < S64_MIN - intermediate_addend)
		return curves_saturate_s64(false);

	// Return sum scaled to output precision.
	return curves_fixed_rescale_s64(intermediate_augend +
						intermediate_addend,
					max_frac_bits, output_frac_bits);
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
	return curves_fixed_narrow_s128_s64(curves_fixed_rescale_s128(
		(s128)multiplicand * (s128)multiplier,
		multiplicand_frac_bits + multiplier_frac_bits,
		output_frac_bits));
}

// ----------------------------------------------------------------------------
// Division
// ----------------------------------------------------------------------------

s64 __cold __curves_fixed_divide_error(s64 dividend, s64 divisor);

// Finds a 128-bit left shift to apply to a dividend before dividing to
// maximize precision without saturating.
//
// Returns: maximum left shift that guarantees division result fits into an
// s64.
static inline int __curves_fixed_divide_optimal_shift(s64 dividend, s64 divisor)
{
	// Convert to magnitudes in u64 for clz.
	u64 abs_dividend = dividend < 0 ? -(u64)dividend : (u64)dividend;
	u64 abs_divisor = divisor < 0 ? -(u64)divisor : (u64)divisor;

	// Find maximum shift where quotient still fits in s64.
	//
	// 62 ensures the quotient magnitude is < 2^63, preventing division
	// from trapping.
	//
	// The | 1 is because clz64(0) is UB. This allows the divisor to be
	// 0 without using a conditional. The final result will still be 0.
	return 62 + (int)clz64(abs_dividend | 1) - (int)clz64(abs_divisor);
}

/**
 * curves_fixed_divide() - Divides two variable-precision, fixed-point values.
*/
static inline s64 curves_fixed_divide(s64 dividend,
				      unsigned int dividend_frac_bits,
				      s64 divisor,
				      unsigned int divisor_frac_bits,
				      unsigned int output_frac_bits)
{
	int optimal_shift, quotient_frac_bits, remaining_shift;
	s64 quotient;

	// Validate inputs.
	if (unlikely(dividend_frac_bits >= 64 || divisor_frac_bits >= 64 ||
		     output_frac_bits >= 64 || divisor == 0))
		return __curves_fixed_divide_error(dividend, divisor);

	// Find maximum shift to apply before division to avoid overflow.
	optimal_shift = __curves_fixed_divide_optimal_shift(dividend, divisor);
	if (unlikely(optimal_shift < 0))
		return curves_saturate_s64((dividend ^ divisor) >= 0);

	// Calculate the precision we'll have after division.
	quotient_frac_bits = (int)dividend_frac_bits + optimal_shift -
			     (int)divisor_frac_bits;

	// Check if the remaining shift is within the valid range.
	remaining_shift = (int)output_frac_bits - quotient_frac_bits;
	if (unlikely(remaining_shift > 63)) {
		return curves_saturate_s64((dividend ^ divisor) >= 0);
	} else if (unlikely(remaining_shift < -63)) {
		return 0;
	}

	// Divide with optimal shift to maximize intermediate precision.
	quotient =
		curves_div_s128_s64((s128)dividend << optimal_shift, divisor);

	// Apply remaining shift to reach target precision.
	if (remaining_shift > 0) {
		return __curves_fixed_shl_sat_s64(
			quotient, (unsigned int)remaining_shift);
	} else if (remaining_shift < 0) {
		return __curves_fixed_shr_rtz_s64(
			quotient, (unsigned int)-remaining_shift);
	} else {
		return quotient;
	}
}

#endif /* _CURVES_FIXED_H */
