// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#include "kernel_compat.h"

extern bool curves_shl_sat_s64(s64 value, unsigned int shift, s64 *result);
extern bool curves_shl_sat_u64(u64 value, unsigned int shift, u64 *result);
extern s64 curves_abs64(s64 x);
extern unsigned int curves_clz32(u32 x);
extern unsigned int curves_clz64(u64 x);
extern u64 curves_int_sqrt(u64 x);
