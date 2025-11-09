// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#include "math64.h"

extern int64_t curves_mul_i64_i64_shr(int64_t multiplicand, int64_t multiplier,
				      unsigned int shift);

extern int64_t curves_div_i64_i64_shl(int64_t numerator, int64_t denominator,
				      unsigned int shift);
