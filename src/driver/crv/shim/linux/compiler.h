// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*!
    \file
    \brief shim header for linux/compiler.h

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#define __cold
#define __maybe_unused __attribute__((unused))
#define __packed       __attribute__((packed))

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#define check_add_overflow(a, b, d) __builtin_add_overflow(a, b, d)
#define check_sub_overflow(a, b, d) __builtin_sub_overflow(a, b, d)
#define check_mul_overflow(a, b, d) __builtin_mul_overflow(a, b, d)
