// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Type compatibility module to allow use in kernel and in user mode code.
 *
 * Copyright (C) 2025 Frank Secilia
 * Author: Frank Secilia <frank.secilia@gmail.com>
 */

#ifndef _CURVES_TYPES_H
#define _CURVES_TYPES_H

#if defined __KERNEL__

#include <linux/kernel.h>
#include <linux/types.h>
#include <asm/bug.h>

#define INT64_MIN S64_MIN
#define INT64_MAX S64_MAX

#else

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define __cold

#if defined(__GNUC__)
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

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

#endif

#endif /* _CURVES_TYPES_H */
