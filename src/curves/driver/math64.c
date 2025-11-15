// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#include "math64.h"

extern s64 curves_saturate_s64(bool result_positive);
extern int64_t curves_div_s128_s64(int128_t dividend, int64_t divisor);

int64_t __cold __curves_rescale_error_s64(int64_t value, int shift)
{
	// If the value is 0 or shift would underflow, return 0.
	if (value == 0 || shift < 0)
		return 0;

	// This would overflow. Saturate based on sign of product.
	return curves_saturate_s64(value >= 0);
}

int64_t __cold __curves_rescale_error_s128(int128_t value, int shift)
{
	// If the value is 0 or shift would underflow, return 0.
	if (value == 0 || shift < 0)
		return 0;

	// This would overflow. Saturate based on sign of product.
	return curves_saturate_s64(value >= 0);
}
