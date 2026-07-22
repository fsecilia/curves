// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "abi.h"
#include <linux/compiler.h>

_Static_assert(sizeof(crv_s8_t) == sizeof(s8), "crv_s8_t size mismatch");
_Static_assert(sizeof(crv_s16_t) == sizeof(s16), "crv_s16_t size mismatch");
_Static_assert(sizeof(crv_s32_t) == sizeof(s32), "crv_s32_t size mismatch");
_Static_assert(sizeof(crv_s64_t) == sizeof(s64), "crv_s64_t size mismatch");

_Static_assert(sizeof(crv_u8_t) == sizeof(u8), "crv_u8_t size mismatch");
_Static_assert(sizeof(crv_u16_t) == sizeof(u16), "crv_u16_t size mismatch");
_Static_assert(sizeof(crv_u32_t) == sizeof(u32), "crv_u32_t size mismatch");
_Static_assert(sizeof(crv_u64_t) == sizeof(u64), "crv_u64_t size mismatch");

#define CRV_IS_SIGNED_TYPE(type) (((type) - 1) < (type)1)

_Static_assert(CRV_IS_SIGNED_TYPE(crv_s8_t) == CRV_IS_SIGNED_TYPE(s8), "crv_s8_t sign mismatch");
_Static_assert(CRV_IS_SIGNED_TYPE(crv_s16_t) == CRV_IS_SIGNED_TYPE(s16), "crv_s16_t sign mismatch");
_Static_assert(CRV_IS_SIGNED_TYPE(crv_s32_t) == CRV_IS_SIGNED_TYPE(s32), "crv_s32_t sign mismatch");
_Static_assert(CRV_IS_SIGNED_TYPE(crv_s64_t) == CRV_IS_SIGNED_TYPE(s64), "crv_s64_t sign mismatch");

_Static_assert(CRV_IS_SIGNED_TYPE(crv_u8_t) == CRV_IS_SIGNED_TYPE(u8), "crv_u8_t sign mismatch");
_Static_assert(CRV_IS_SIGNED_TYPE(crv_u16_t) == CRV_IS_SIGNED_TYPE(u16), "crv_u16_t sign mismatch");
_Static_assert(CRV_IS_SIGNED_TYPE(crv_u32_t) == CRV_IS_SIGNED_TYPE(u32), "crv_u32_t sign mismatch");
_Static_assert(CRV_IS_SIGNED_TYPE(crv_u64_t) == CRV_IS_SIGNED_TYPE(u64), "crv_u64_t sign mismatch");
