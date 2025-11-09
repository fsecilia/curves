// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Fixed-point number type and supporting math functions.
 *
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#ifndef _CURVES_FIXED_H
#define _CURVES_FIXED_H

#include "types.h"

typedef int64_t curves_fixed_t;

static inline curves_fixed_t curves_const_one(unsigned int decimal_place)
{
	return 1ll << decimal_place;
}

#define CURVES_LN_2_DECIMAL_PLACE 62
static inline curves_fixed_t curves_const_ln2(unsigned int decimal_place)
{
	// This value was generated using wolfram alpha: round(log(2)*2^62)
	return 3196577161300663915ll >>
	       (CURVES_LN_2_DECIMAL_PLACE - decimal_place);
}

#endif /* _CURVES_FIXED_h */
