// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief common linux c++ abi
///
/// C++ code can't include Linux headers directly, so types are redeclared here, then asserted in abi.c for bitwise
/// compatibility.
///
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

typedef __INT8_TYPE__ crv_s8_t;
typedef __INT16_TYPE__ crv_s16_t;
typedef __INT32_TYPE__ crv_s32_t;
typedef __INT64_TYPE__ crv_s64_t;

typedef __UINT8_TYPE__ crv_u8_t;
typedef __UINT16_TYPE__ crv_u16_t;
typedef __UINT32_TYPE__ crv_u32_t;
typedef __UINT64_TYPE__ crv_u64_t;

#if !defined __KERNEL__
#define __user
#endif
