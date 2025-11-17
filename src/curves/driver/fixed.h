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

s64 __cold __curves_fixed_rescale_error_s64(s64 value, unsigned int frac_bits,
					    unsigned int output_frac_bits);

// Shifts right, rounding towards zero.
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
static inline s64 __curves_fixed_shl_sat_s64(s64 value, unsigned int shift)
{
	// Find the maximum value that doesn't overflow.
	s64 max_safe_val = S64_MAX >> shift;
	if (unlikely(value > max_safe_val))
		return S64_MAX;

	// Find the minimum value that doesn't overflow.
	s64 min_safe_val = S64_MIN >> shift;
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
	// Handle invalid scales.
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

s128 __cold __curves_fixed_rescale_error_s128(s128 value,
					      unsigned int frac_bits,
					      unsigned int output_frac_bits);

// Shifts right, rounding towards zero.
static inline s128 __curves_fixed_shr_rtz_s128(s128 value, unsigned int shift)
{
	// To round up during division, bias dividend by divisor - 1.
	s128 divisor = 1LL << shift;
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
	// Find the maximum value that doesn't overflow.
	s128 max_safe_val = CURVES_S128_MAX >> shift;
	if (unlikely(value > max_safe_val))
		return CURVES_S128_MAX;

	// Find the minimum value that doesn't overflow.
	s128 min_safe_val = CURVES_S128_MIN >> shift;
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

/**
 * curves_fixed_multiply() - Multiplies two arbitrary-precision fixed-point
 * values.
 *
 * @multiplicand_frac_bits: Fractional bits in multiplicand, [0, 62].
 * @multiplicand: Value to multiply.
 * @multiplier_frac_bits: Fractional bits in multiplier, [0, 62].
 * @multiplier: Amount to multiply by.
 * @output_frac_bits: Fractional bits in result, [0, 62].
 *
 * This function multiplies two fixed-point values with independent fractional
 * precision and shifts the result to match @output_frac_bits. The raw product
 * has @multiplicand_frac_bits + @multiplier_frac_bits fractional bits; this
 * function shifts it left or right as needed to produce the requested output
 * precision.
 *
 * The shift and multiply are done at 128 bits before truncating the result to
 * 64 bits.
 *
 * The caller is responsible for ensuring @output_frac_bits leaves sufficient
 * integer bits to represent the product magnitude. No overflow detection is
 * performed.
 *
 * Return: Product shifted to @output_frac_bits precision, or 0 if the
 * required shift would cause undefined behavior (|shift| >= 64 for left
 * shifts, >= 128 for right shifts).
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

s64 __cold __curves_fixed_divide_error(s64 dividend, s64 divisor, int shift);

static inline s64 __curves_fixed_divide_try_saturate(s64 dividend, s64 divisor,
						     s128 threshold)
{
	s128 abs_dividend = dividend < 0 ? -dividend : dividend;
	s128 abs_threshold = threshold < 0 ? -threshold : threshold;

	if (unlikely(abs_dividend >= abs_threshold)) {
		// Saturate based on sign of quotient.
		return curves_saturate_s64((dividend ^ divisor) >= 0);
	}

	return 0;
}

static inline s64 __curves_fixed_divide_try_saturate_shl(s64 dividend,
							 s64 divisor, int shift)
{
	s128 saturation_threshold;
	s64 saturation;

	int saturation_threshold_bit = 63 - shift;

	if (saturation_threshold_bit >= 0) {
		// 128-bit shift
		saturation_threshold = (s128)divisor
				       << saturation_threshold_bit;
	} else {
		// 64-bit shift
		saturation_threshold =
			(s128)(divisor >> -saturation_threshold_bit);
	}

	saturation = __curves_fixed_divide_try_saturate(dividend, divisor,
							saturation_threshold);
	if (unlikely(saturation != 0))
		return saturation;

	return 0;
}

static inline s64 __curves_fixed_divide_try_saturate_shr(s64 dividend,
							 s64 divisor, int shift)
{
	int saturation_threshold_bit = 63 + shift;

	s128 saturation_threshold = (s128)divisor << saturation_threshold_bit;
	s64 saturation = __curves_fixed_divide_try_saturate(
		dividend, divisor, saturation_threshold);

	if (unlikely(saturation != 0))
		return saturation >> -shift;

	return 0;
}

static inline s64 curves_fixed_divide(s64 dividend,
				      unsigned int dividend_frac_bits,
				      s64 divisor,
				      unsigned int divisor_frac_bits,
				      unsigned int output_frac_bits)
{
	int shift = (int)output_frac_bits + (int)divisor_frac_bits -
		    (int)dividend_frac_bits;

	if (unlikely(shift >= 128 || shift <= -64 || divisor == 0))
		return __curves_fixed_divide_error(dividend, divisor, shift);

	if (shift >= 0) {
		s64 saturation = __curves_fixed_divide_try_saturate_shl(
			dividend, divisor, shift);
		if (unlikely(saturation != 0))
			return saturation;

		return curves_div_s128_s64((s128)dividend << shift, divisor);
	} else {
		s64 saturation = __curves_fixed_divide_try_saturate_shr(
			dividend, divisor, shift);
		if (unlikely(saturation != 0))
			return saturation;

		return curves_fixed_rescale_s64(
			curves_div_s128_s64(dividend, divisor),
			divisor_frac_bits - dividend_frac_bits,
			output_frac_bits);
	}
}

#endif /* _CURVES_FIXED_H */
