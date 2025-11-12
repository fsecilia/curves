// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Fixed-point number type and supporting math functions.
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
 * curves_const_1() - Fixed-point constant 1
 * @frac_bits: Fractional bit precision (0 to 63)
 *
 * Return: Constant value 1 with specified precision
 */
static inline curves_fixed_t curves_const_1(unsigned int frac_bits)
{
	return 1ll << frac_bits;
}

/**
 * curves_const_e() - Fixed-point constant e
 * @frac_bits: Fractional bit precision (0 to CURVES_E_FRAC_BITS)
 *
 * Return: Constant value e with specified precision
 */
#define CURVES_E_FRAC_BITS 61
static inline curves_fixed_t curves_const_e(unsigned int frac_bits)
{
	// This value was generated using wolfram alpha: round(e*2^61)
	return 6267931151224907085ll >> (CURVES_E_FRAC_BITS - frac_bits);
}

/**
 * curves_const_ln2() - Fixed-point constant ln(2)
 * @frac_bits: Fractional bit precision (0 to CURVES_LN2_FRAC_BITS)
 *
 * Return: Constant value ln(2) with specified precision
 */
#define CURVES_LN2_FRAC_BITS 62
static inline curves_fixed_t curves_const_ln2(unsigned int frac_bits)
{
	// This value was generated using wolfram alpha: round(log(2)*2^62)
	return 3196577161300663915ll >> (CURVES_LN2_FRAC_BITS - frac_bits);
}

/**
 * curves_const_pi() - Fixed-point constant pi
 * @frac_bits: Fractional bit precision (0 to CURVES_PI_FRAC_BITS)
 *
 * Return: Constant value pi with specified precision
 */
#define CURVES_PI_FRAC_BITS 61
static inline curves_fixed_t curves_const_pi(unsigned int frac_bits)
{
	// This value was generated using wolfram alpha: round(pi*2^61)
	return 7244019458077122842ll >> (CURVES_PI_FRAC_BITS - frac_bits);
}

/**
 * curves_fixed_from_integer() - Converts integers to fixed point
 * @frac_bits: Fractional bit precision (0 to 62)
 * @value: Integer to convert
 *
 * Return: value in fixed-point with given precision
 */
static inline curves_fixed_t curves_fixed_from_integer(unsigned int frac_bits,
						       int64_t value)
{
	return value << frac_bits;
}

/**
 * curves_fixed_to_integer() - Converts fixed point to integers
 * @frac_bits: Fractional bit precision (0 to 62)
 * @value: Fixed-point with given precision to convert
 *
 * Return: value as an integer
 */
static inline int64_t curves_fixed_to_integer(unsigned int frac_bits,
					      curves_fixed_t value)
{
	return value >> frac_bits;
}

/**
 * curves_fixed_multiply() - Multiply two arbitrary-precision fixed-point values
 * @multiplicand_frac_bits: Fractional bits in multiplicand, [0, 62]
 * @multiplicand: Value to multiply
 * @multiplier_frac_bits: Fractional bits in multiplier, [0, 62]
 * @multiplier: Amount to multiply by
 * @output_frac_bits: Fractional bits in result, [0, 62]
 *
 * Multiplies two fixed-point values with independent fractional precision
 * and shifts the result to match @output_frac_bits. The raw product has
 * @multiplicand_frac_bits + @multiplier_frac_bits fractional bits; this
 * function shifts it left or right as needed to produce the requested
 * output precision.
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

	// Shift from where the result lands to where result is requested.
	int shift = (int)output_frac_bits - (int)product_frac_bits;

	// Prevent UB from extreme shifts.
	// Both of these shifts induce UB, but even if they didn't, they would
	// unconditionally shift the entire result out of the final 64 bits.
	if (unlikely(shift >= 64 || shift <= -128)) {
		WARN_ON_ONCE(shift >= 64 || shift <= -128);
		return 0;
	}

	// Shift and let caller ensure precision bounds prevent overflow.
	return (curves_fixed_t)(shift > 0 ? product << shift :
					    product >> -shift);
}

#endif /* _CURVES_FIXED_H */
