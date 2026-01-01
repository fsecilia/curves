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
#include <linux/math.h>

static inline bool curves_shl_sat_s64(s64 value, unsigned int shift,
				      s64 *result)
{
	if (unlikely(check_shl_overflow(value, shift, result))) {
		*result = (value < 0) ? S64_MIN : S64_MAX;
		return true;
	}
	return false;
}

static inline bool curves_shl_sat_u64(u64 value, unsigned int shift,
				      u64 *result)
{
	if (unlikely(check_shl_overflow(value, shift, result))) {
		*result = U64_MAX;
		return true;
	}
	return false;
}

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

static inline u64 curves_int_sqrt(u64 x)
{
	return int_sqrt64(x);
}

#else

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>

typedef int8_t s8;
typedef uint8_t u8;
typedef int16_t s16;
typedef uint16_t u16;
typedef int32_t s32;
typedef uint32_t u32;
typedef int64_t s64;
typedef uint64_t u64;
__extension__ typedef __int128 s128;
__extension__ typedef unsigned __int128 u128;

#define BITS_PER_BYTE CHAR_BIT

#define S8_MIN INT8_MIN
#define S8_MAX INT8_MAX
#define S16_MIN INT16_MIN
#define S16_MAX INT16_MAX
#define S32_MIN INT32_MIN
#define S32_MAX INT32_MAX
#define S64_MIN INT64_MIN
#define S64_MAX INT64_MAX
#define U8_MIN UINT8_MIN
#define U8_MAX UINT8_MAX
#define U16_MIN UINT16_MIN
#define U16_MAX UINT16_MAX
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

#define __maybe_unused __attribute__((unused))

#define check_add_overflow(a, b, d) __builtin_add_overflow(a, b, d)
#define check_sub_overflow(a, b, d) __builtin_sub_overflow(a, b, d)
#define check_mul_overflow(a, b, d) __builtin_mul_overflow(a, b, d)

static inline bool curves_shl_sat_s64(s64 value, unsigned int shift,
				      s64 *result)
{
	if (unlikely(shift >= 64)) {
		*result = (value < 0) ? S64_MIN : S64_MAX;
		return true;
	}

	if (unlikely(value > (S64_MAX >> shift))) {
		*result = S64_MAX;
		return true;
	}

	if (unlikely(value < (S64_MIN >> shift))) {
		*result = S64_MIN;
		return true;
	}

	*result = value << shift;

	return false;
}

static inline bool curves_shl_sat_u64(u64 value, unsigned int shift,
				      u64 *result)
{
	if (unlikely(shift >= 64)) {
		*result = U64_MAX;
		return true;
	}

	if (unlikely(value > (U64_MAX >> shift))) {
		*result = U64_MAX;
		return true;
	}

	*result = value << shift;

	return false;
}

#define WARN_ONCE(condition, format, ...)                             \
	({                                                            \
		bool __ret = (condition);                             \
		if (__ret) {                                          \
			static bool _warned_once = false;             \
			if (!_warned_once) {                          \
				_warned_once = true;                  \
				fprintf(stderr, "WARNING at %s:%d\n", \
					__FILE__, __LINE__);          \
			}                                             \
		}                                                     \
		__ret;                                                \
	})

#define WARN_ON_ONCE(condition) \
	WARN_ONCE(condition, "WARNING at %s:%d\n", __FILE__, __LINE__)

#ifdef __cplusplus
#define BUILD_BUG_ON(condition) \
	static_assert(!(condition), "BUILD_BUG_ON: " #condition)
#else
#define BUILD_BUG_ON(condition) \
	_Static_assert(!(condition), "BUILD_BUG_ON: " #condition)
#endif

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

static inline u64 curves_int_sqrt(u64 x)
{
	return (u64)sqrt((long double)x);
}

#endif

#endif /* _CURVES_TYPES_H */
