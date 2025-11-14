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

typedef int64_t curves_fixed_t;

/**
 * curves_const_1() - Fixed-point constant 1.
 * @frac_bits: Fractional bit precision, [0 to 63].
 *
 * Return: Constant value 1 with specified precision.
 */
static inline curves_fixed_t curves_const_1(unsigned int frac_bits)
{
	return 1ll << frac_bits;
}

/**
 * curves_const_e() - Fixed-point constant e.
 * @frac_bits: Fractional bit precision, [0 to CURVES_E_FRAC_BITS].
 *
 * Return: Constant value e with specified precision.
 */
#define CURVES_E_FRAC_BITS 61
static inline curves_fixed_t curves_const_e(unsigned int frac_bits)
{
	// This value was generated using wolfram alpha: round(e*2^61)
	return 6267931151224907085ll >> (CURVES_E_FRAC_BITS - frac_bits);
}

/**
 * curves_const_ln2() - Fixed-point constant ln(2).
 * @frac_bits: Fractional bit precision, [0 to CURVES_LN2_FRAC_BITS].
 *
 * Return: Constant value ln(2) with specified precision.
 */
#define CURVES_LN2_FRAC_BITS 62
static inline curves_fixed_t curves_const_ln2(unsigned int frac_bits)
{
	// This value was generated using wolfram alpha: round(log(2)*2^62)
	return 3196577161300663915ll >> (CURVES_LN2_FRAC_BITS - frac_bits);
}

/**
 * curves_const_pi() - Fixed-point constant pi.
 * @frac_bits: Fractional bit precision, [0 to CURVES_PI_FRAC_BITS].
 *
 * Return: Constant value pi with specified precision.
 */
#define CURVES_PI_FRAC_BITS 61
static inline curves_fixed_t curves_const_pi(unsigned int frac_bits)
{
	// This value was generated using wolfram alpha: round(pi*2^61)
	return 7244019458077122842ll >> (CURVES_PI_FRAC_BITS - frac_bits);
}

/**
 * curves_fixed_from_integer() - Converts integers to fixed point
 * @frac_bits: Fractional bit precision, [0, 62].
 * @value: Integer to convert
 *
 * Return: value in fixed-point with given precision.
 */
static inline curves_fixed_t curves_fixed_from_integer(unsigned int frac_bits,
						       int64_t value)
{
	return value << frac_bits;
}

/**
 * curves_fixed_to_integer() - Converts fixed point to integers
 * @frac_bits: Fractional bit precision, [0, 62].
 * @value: Fixed-point with given precision to convert
 *
 * Return: value as an integer.
 */
static inline int64_t curves_fixed_to_integer(unsigned int frac_bits,
					      curves_fixed_t value)
{
	return value >> frac_bits;
}

curves_fixed_t __cold __curves_fixed_multiply_error(curves_fixed_t multiplicand,
						    curves_fixed_t multiplier,
						    int shift);

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
static inline curves_fixed_t
curves_fixed_multiply(unsigned int multiplicand_frac_bits,
		      curves_fixed_t multiplicand,
		      unsigned int multiplier_frac_bits,
		      curves_fixed_t multiplier, unsigned int output_frac_bits)
{
	// Intermediate product comes from regular multiplication.
	int128_t product = (int128_t)multiplicand * (int128_t)multiplier;

	// Calculate final shift to align binary point with output_frac_bits.
	int shift = (int)output_frac_bits - (int)multiplicand_frac_bits -
		    (int)multiplier_frac_bits;

	// Handle UB shifts.
	if (unlikely(shift >= 64 || shift <= -128))
		return __curves_fixed_multiply_error(multiplicand, multiplier,
						     shift);

	// Execute signed shift.
	if (shift >= 0) {
		// Left shift truncates because low bits are already 0.
		return curves_s128_to_s64_truncate(product << shift);
	} else {
		// Right shift must round lost bits.
		return curves_s128_to_s64_shr_rtn(product,
						  (unsigned int)(-shift));
	}
}

curves_fixed_t __cold __curves_fixed_divide_error(curves_fixed_t dividend,
						  curves_fixed_t divisor,
						  int shift);

static inline curves_fixed_t
__curves_fixed_divide_try_saturate(curves_fixed_t dividend,
				   curves_fixed_t divisor, int128_t threshold)
{
	int128_t abs_dividend = dividend < 0 ? -dividend : dividend;
	int128_t abs_threshold = threshold < 0 ? -threshold : threshold;

	if (abs_dividend >= abs_threshold) {
		// Saturate based on sign of quotient.
		return curves_s64_saturate((dividend ^ divisor) >= 0);
	}

	return 0;
}

static inline curves_fixed_t
__curves_fixed_divide_try_saturate_lshift(curves_fixed_t dividend,
					  curves_fixed_t divisor, int shift)
{
	int128_t saturation_threshold;
	curves_fixed_t saturation;

	int saturation_threshold_bit = 63 - shift;

	if (saturation_threshold_bit >= 0) {
		// 128-bit shift
		saturation_threshold = (int128_t)divisor
				       << saturation_threshold_bit;
	} else {
		// 64-bit shift
		saturation_threshold =
			(int128_t)(divisor >> -saturation_threshold_bit);
	}

	saturation = __curves_fixed_divide_try_saturate(dividend, divisor,
							saturation_threshold);
	if (saturation != 0)
		return saturation;

	return 0;
}

static inline curves_fixed_t
__curves_fixed_divide_try_saturate_rshift(curves_fixed_t dividend,
					  curves_fixed_t divisor, int shift)
{
	int saturation_threshold_bit = 63 + shift;

	int128_t saturation_threshold = (int128_t)divisor
					<< saturation_threshold_bit;
	curves_fixed_t saturation = __curves_fixed_divide_try_saturate(
		dividend, divisor, saturation_threshold);

	if (saturation != 0)
		return saturation;

	return 0;
}

static inline curves_fixed_t
curves_fixed_divide(unsigned int dividend_frac_bits, curves_fixed_t dividend,
		    unsigned int divisor_frac_bits, curves_fixed_t divisor,
		    unsigned int output_frac_bits)
{
	int shift = (int)output_frac_bits + (int)divisor_frac_bits -
		    (int)dividend_frac_bits;

	if (unlikely(shift >= 128 || shift <= -64 || divisor == 0))
		return __curves_fixed_divide_error(dividend, divisor, shift);

	if (shift >= 0) {
		curves_fixed_t saturation =
			__curves_fixed_divide_try_saturate_lshift(
				dividend, divisor, shift);
		if (saturation != 0)
			return saturation;

		return curves_div_s128_by_s64_rtn((int128_t)dividend << shift,
						  divisor);
	} else {
		curves_fixed_t quotient;

		unsigned int right_shift = (unsigned int)(-shift);
		curves_fixed_t saturation =
			__curves_fixed_divide_try_saturate_rshift(
				dividend, divisor, shift);
		if (saturation != 0)
			return saturation >> right_shift;

		// Divide, applying rtn to the remainder.
		quotient = curves_div_s128_by_s64_rtn(dividend, divisor);

		// Shift, applying rtn to the bits that are lost.
		return curves_s64_shr_rtn(quotient, right_shift);
	}
}

#endif /* _CURVES_FIXED_H */
