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

typedef int64_t curves_fixed_t;

static inline curves_fixed_t curves_const_one(unsigned int frac_bits)
{
	return 1ll << frac_bits;
}

#define CURVES_E_FRAC_BITS 61
static inline curves_fixed_t curves_const_e(unsigned int frac_bits)
{
	// This value was generated using wolfram alpha: round(e*2^61)
	return 6267931151224907085ll >> (CURVES_E_FRAC_BITS - frac_bits);
}

#define CURVES_LN_2_FRAC_BITS 62
static inline curves_fixed_t curves_const_ln2(unsigned int frac_bits)
{
	// This value was generated using wolfram alpha: round(log(2)*2^62)
	return 3196577161300663915ll >> (CURVES_LN_2_FRAC_BITS - frac_bits);
}

#define CURVES_PI_FRAC_BITS 61
static inline curves_fixed_t curves_const_pi(unsigned int frac_bits)
{
	// This value was generated using wolfram alpha: round(pi*2^61)
	return 7244019458077122842ll >> (CURVES_PI_FRAC_BITS - frac_bits);
}

#endif /* _CURVES_FIXED_H */
