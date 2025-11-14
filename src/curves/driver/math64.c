// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#include "math64.h"

extern s64 curves_s64_saturate(bool result_positive);
extern s64 curves_s128_to_s64_truncate(s128 value);

extern s64 curves_s64_shr_rtn(s64 value, unsigned int right_shift);
extern s64 curves_s128_to_s64_shr_rtn(s128 value, unsigned int right_shift);

extern struct curves_div_result curves_div_s128_by_s64(int128_t dividend,
						       int64_t divisor);
extern s64 curves_div_s128_by_s64_rtn(int128_t dividend, s64 divisor);
