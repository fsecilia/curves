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
 * curves_fixed_multiply() - Multiplies two fixed-point numbers of arbitrary
 * precision
 * @multiplicand_frac_bits: Fractional bit precision of multiplicand (0 to 62)
 * @multiplicand: Value to multiply
 * @multiplier_frac_bits: Fractional bit precision of multiplier (0 to 62)
 * @multiplier: Amount to multiply by
 * @output_frac_bits: Fractional bit precision of final output (0 to 62)
 *
 * This function takes two fixed-point values, each with their own arbitrary
 * precision, then produces their product at another precision. Precisions sum,
 * such that if you have a multiplicand in format qA.B and a multiplier in
 * format qC.D, their product will be in format q(A+B).(C+D). Typically, the
 * result must be shifted right to bring it back into a usable range. Here, the
 * result will be shifted so the precision matches exactly output_frac_bits. It
 * will shift left if necessary. This way, the output of one calculation can
 * chain directly into another with the expected precision, regardless of the
 * precision of the inputs that produced it.
 *
 * Internally, the multiplication and shift are done with 128-bits, before
 * truncating back to 64.
 *
 * Only the fractional bits are aligned to output_frac_bits. The integer bits
 * may still overflow. The caller must ensure output_frac_bits is appropriate
 * for the magnitude of multiplicand * multiplier. No overflow checking is
 * performed on the result value.
 *
 * If the size of the shift to output_frac_bits would induce UB, the result is
 * 0.
 */
static inline curves_fixed_t
curves_fixed_multiply(unsigned int multiplicand_frac_bits,
		      curves_fixed_t multiplicand,
		      unsigned int multiplier_frac_bits,
		      curves_fixed_t multiplier, unsigned int output_frac_bits)
{
	int128_t product = (int128_t)multiplicand * (int128_t)multiplier;
	unsigned int product_frac_bits =
		multiplicand_frac_bits + multiplier_frac_bits;

	int shift = (int)output_frac_bits - (int)product_frac_bits;

	// Prevent UB from extreme shifts.
	if (shift >= 64 || shift <= -128)
		return 0;

	// Otherwise shift and let caller ensure precision bounds prevent
	// overflow.
	return (curves_fixed_t)(shift > 0 ? product << shift :
					    product >> -shift);
}

#endif /* _CURVES_FIXED_H */
