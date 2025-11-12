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

	// Under multiplication, precisions sum because they are logs.
	unsigned int product_frac_bits =
		multiplicand_frac_bits + multiplier_frac_bits;

	if (output_frac_bits > product_frac_bits) {
		unsigned int shift = output_frac_bits - product_frac_bits;

		// Handle large shifts.
		if (unlikely(shift >= 64)) {
			WARN_ON_ONCE(shift >= 64);

			// 0 stays 0.
			if (multiplicand == 0 || multiplier == 0)
				return 0;

			// Saturate based on sign of result.
			return (multiplicand > 0) == (multiplier > 0) ?
				       INT64_MAX :
				       INT64_MIN;
		}

		return product << shift;
	} else {
		unsigned int shift = product_frac_bits - output_frac_bits;

		// Handle large shifts.
		if (unlikely(shift >= 128)) {
			WARN_ON_ONCE(shift >= 128);
			return 0;
		}

		return product >> shift;
	}
}

static inline curves_fixed_t
curves_fixed_divide(unsigned int dividend_frac_bits, curves_fixed_t dividend,
		    unsigned int divisor_frac_bits, curves_fixed_t divisor,
		    unsigned int output_frac_bits)
{
	int shift = (int)divisor_frac_bits + (int)output_frac_bits -
		    (int)dividend_frac_bits;

	// Handle dividing by 0.
	if (divisor == 0) {
		/*
		 * Both are 0, completely undefined result. Arbitrarily choose
		 * to return 0.
		*/
		if (dividend == 0)
			return 0;

		// Saturate based on sign of result.
		return dividend < 0 ? INT64_MIN : INT64_MAX;
	}

	// Handle large shifts.
	if (shift >= 128) {
		// 0 stays 0.
		if (dividend == 0)
			return 0;

		// Saturate based on sign of result.
		return (dividend > 0) == (divisor > 0) ? INT64_MAX : INT64_MIN;
	}

	if (shift > 0)
		return ((int128_t)dividend << shift) / divisor;
	else
		return ((int128_t)dividend / divisor) >> shift;
}

#endif /* _CURVES_FIXED_H */
