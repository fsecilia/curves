// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "abi.h"
#include <linux/compiler.h>
#include <linux/input.h>
#include <linux/stddef.h>

//
// fundamental types
//

_Static_assert(sizeof(crv_input_s32_t) == sizeof(s32), "crv_input_s32_t size mismatch");
_Static_assert(sizeof(crv_input_s64_t) == sizeof(s64), "crv_input_s64_t size mismatch");
_Static_assert(sizeof(crv_input_u16_t) == sizeof(u16), "crv_input_u16_t size mismatch");
_Static_assert(sizeof(crv_input_u64_t) == sizeof(u64), "crv_input_u64_t size mismatch");

#define CRV_IS_SIGNED_TYPE(type) (((type) - 1) < (type)1)
_Static_assert(CRV_IS_SIGNED_TYPE(crv_input_s32_t) == CRV_IS_SIGNED_TYPE(s32), "crv_input_s32_t sign mismatch");
_Static_assert(CRV_IS_SIGNED_TYPE(crv_input_s64_t) == CRV_IS_SIGNED_TYPE(s64), "crv_input_s64_t sign mismatch");
_Static_assert(CRV_IS_SIGNED_TYPE(crv_input_u16_t) == CRV_IS_SIGNED_TYPE(u16), "crv_input_u16_t sign mismatch");
_Static_assert(CRV_IS_SIGNED_TYPE(crv_input_u64_t) == CRV_IS_SIGNED_TYPE(u64), "crv_input_u64_t sign mismatch");

//
// crv_input_value_t
//

_Static_assert(sizeof(struct crv_input_value_t) == sizeof(struct input_value), "input_value size mismatch");
_Static_assert(_Alignof(struct crv_input_value_t) == _Alignof(struct input_value), "input_value alignment mismatch");

_Static_assert(offsetof(struct crv_input_value_t, type) == offsetof(struct input_value, type),
    "input_value::type offset mismatch");

_Static_assert(offsetof(struct crv_input_value_t, code) == offsetof(struct input_value, code),
    "input_value::code offset mismatch");

_Static_assert(offsetof(struct crv_input_value_t, value) == offsetof(struct input_value, value),
    "input_value::value offset mismatch");

_Static_assert(__same_type(((struct crv_input_value_t*)0)->type, ((struct input_value*)0)->type),
    "input_value::type type mismatch");

_Static_assert(__same_type(((struct crv_input_value_t*)0)->code, ((struct input_value*)0)->code),
    "input_value::code type mismatch");

_Static_assert(__same_type(((struct crv_input_value_t*)0)->value, ((struct input_value*)0)->value),
    "input_value::value type mismatch");

_Static_assert(sizeof(struct crv_input_value_t*) == 8, "input_value size mismatch");

//
// constants
//

_Static_assert(CRV_EV_SYN == EV_SYN, "EV_SYN mismatch");
_Static_assert(CRV_EV_REL == EV_REL, "EV_REL mismatch");
_Static_assert(CRV_SYN_REPORT == SYN_REPORT, "SYN_REPORT mismatch");
_Static_assert(CRV_REL_X == REL_X, "REL_X mismatch");
_Static_assert(CRV_REL_Y == REL_Y, "REL_Y mismatch");
