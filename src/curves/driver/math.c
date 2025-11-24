// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#include "math.h"

extern s64 curves_sign_mask(s64 value);
extern u64 curves_strip_sign(s64 value);
extern s64 curves_apply_sign(u64 value, s64 mask);

extern s64 curves_fixed_narrow_s128_s64(s128 value);

extern struct div_u128_u64_result
curves_div_u128_u64(unsigned __int128 dividend, u64 divisor);
