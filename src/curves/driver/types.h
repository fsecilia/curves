// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Type compatibility module to allow use in kernal and in user mode code.
 *
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#ifndef _CURVES_TYPES_H
#define _CURVES_TYPES_H

#if defined __KERNEL__
#include <linux/kernel.h>
#include <linux/types.h>
#else
#include <stdint.h>
#define S64_MIN INT64_MIN
#define S64_MAX INT64_MAX
#endif

#if !(defined __SIZEOF_INT128__ && __SIZEOF_INT128__ == 16)
#error This module requires 128-bit integer types, but they are not available.
#endif

__extension__ typedef __int128 int128_t;
__extension__ typedef unsigned __int128 uint128_t;

#endif /* _CURVES_TYPES_H */
