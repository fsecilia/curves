// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Compatibility module to allow using fixed-point code in kernel and in user
 * mode.
 *
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#ifndef _CURVES_TYPES_H
#define _CURVES_TYPES_H

#if !(defined __SIZEOF_INT128__ && __SIZEOF_INT128__ == 16)
#error This module requires 128-bit integer types, but they are not available.
#endif

#if defined __KERNEL__

#include <linux/kernel.h>
#include <linux/types.h>
#include <asm/bug.h>
#include <linux/bitops.h>

static inline s64 curves_abs64(s64 x)
{
	return abs(x);
}

static inline unsigned int curves_clz32(u32 x)
{
	return 32 - fls(x);
}

static inline unsigned int curves_clz64(u64 x)
{
	return 64 - fls64(x);
}

#else

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef int32_t s32;
typedef uint32_t u32;
typedef int64_t s64;
typedef uint64_t u64;
__extension__ typedef __int128 s128;
__extension__ typedef unsigned __int128 u128;

#define S32_MIN INT32_MIN
#define S32_MAX INT32_MAX
#define S64_MIN INT64_MIN
#define S64_MAX INT64_MAX
#define U32_MIN UINT32_MIN
#define U32_MAX UINT32_MAX
#define U64_MIN UINT64_MIN
#define U64_MAX UINT64_MAX

#define __cold

#if defined(__GNUC__)
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

#define check_add_overflow(a, b, d) __builtin_add_overflow(a, b, d)
#define check_sub_overflow(a, b, d) __builtin_sub_overflow(a, b, d)
#define check_mul_overflow(a, b, d) __builtin_mul_overflow(a, b, d)

#define WARN_ONCE(condition, format, ...)                           \
	do {                                                        \
		if (condition) {                                    \
			static bool _warned_once = false;           \
			if (unlikely(!_warned_once)) {              \
				_warned_once = true;                \
				fprintf(stderr, "WARNING: " format, \
					##__VA_ARGS__);             \
			}                                           \
		}                                                   \
	} while (0)

#define WARN_ON_ONCE(condition) \
	WARN_ONCE(condition, "WARNING at %s:%d\n", __FILE__, __LINE__)

static inline s64 curves_abs64(s64 x)
{
	return llabs(x);
}

static inline unsigned int curves_clz32(u32 x)
{
	return x ? (unsigned int)__builtin_clzll(x) : (unsigned int)32;
}

static inline unsigned int curves_clz64(u64 x)
{
	return x ? (unsigned int)__builtin_clzll(x) : (unsigned int)64;
}

#endif

#endif /* _CURVES_TYPES_H */
