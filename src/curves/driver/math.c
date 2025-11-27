// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#include "math.h"

extern s64 curves_saturate_s64(bool result_positive);
extern s128 curves_saturate_s128(bool result_positive);

extern s64 curves_sign_mask(s64 value);
extern u64 curves_strip_sign(s64 value);
extern s64 curves_apply_sign(u64 value, s64 mask);
extern u32 curves_max_u32(u32 a, u32 b);
extern u32 curves_min_u32(u32 a, u32 b);

extern s64 curves_narrow_s128_s64(s128 value);
extern u64 curves_narrow_u128_u64(u128 value);

extern s64 curves_add_saturate(s64 augend, s64 addend);
extern s64 curves_subtract_saturate(s64 minuend, s64 subtrahend);

extern struct div_u128_u64_result curves_div_u128_u64(u128 dividend,
						      u64 divisor);

extern s64 curves_log2_s64(s64 value);
