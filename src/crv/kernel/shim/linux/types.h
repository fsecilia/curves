// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*!
    \file
    \brief shim header for linux/types.h

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include_next <linux/types.h>

#include <stdbool.h>
#include <stdint.h>

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define BITS_PER_BYTE CHAR_BIT

#define S8_MIN  INT8_MIN
#define S8_MAX  INT8_MAX
#define S16_MIN INT16_MIN
#define S16_MAX INT16_MAX
#define S32_MIN INT32_MIN
#define S32_MAX INT32_MAX
#define S64_MIN INT64_MIN
#define S64_MAX INT64_MAX
#define U8_MIN  UINT8_MIN
#define U8_MAX  UINT8_MAX
#define U16_MIN UINT16_MIN
#define U16_MAX UINT16_MAX
#define U32_MIN UINT32_MIN
#define U32_MAX UINT32_MAX
#define U64_MIN UINT64_MIN
#define U64_MAX UINT64_MAX
