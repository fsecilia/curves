// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "abi.h"
#include <linux/compiler.h>
#include <linux/input.h>
#include <linux/stddef.h>

//
// crv_input_value_t
//

_Static_assert(sizeof(struct crv_input_value_t) == sizeof(struct input_value), "input_value size mismatch");
_Static_assert(_Alignof(struct crv_input_value_t) == _Alignof(struct input_value), "input_value alignment mismatch");

CRV_MEMBER_SAME_LAYOUT_INTEGER(struct crv_input_value_t, struct input_value, type);
CRV_MEMBER_SAME_LAYOUT_INTEGER(struct crv_input_value_t, struct input_value, code);
CRV_MEMBER_SAME_LAYOUT_INTEGER(struct crv_input_value_t, struct input_value, value);

//
// constants
//

_Static_assert(CRV_EV_SYN == EV_SYN, "EV_SYN mismatch");
_Static_assert(CRV_EV_REL == EV_REL, "EV_REL mismatch");
_Static_assert(CRV_SYN_REPORT == SYN_REPORT, "SYN_REPORT mismatch");
_Static_assert(CRV_REL_X == REL_X, "REL_X mismatch");
_Static_assert(CRV_REL_Y == REL_Y, "REL_Y mismatch");
