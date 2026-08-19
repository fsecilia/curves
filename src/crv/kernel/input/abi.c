// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "abi.h"
#include <crv/kernel/abi_validation.h>
#include <linux/input.h>

//
// crv_input_value_t
//

CRV_SAME_LAYOUT(struct crv_input_value_t, struct input_value);
CRV_MEMBER_SAME_LAYOUT_INTEGER(struct crv_input_value_t, struct input_value, type);
CRV_MEMBER_SAME_LAYOUT_INTEGER(struct crv_input_value_t, struct input_value, code);
CRV_MEMBER_SAME_LAYOUT_INTEGER(struct crv_input_value_t, struct input_value, value);

//
// constants
//

CRV_STATIC_ASSERT(CRV_EV_SYN == EV_SYN, "EV_SYN mismatch");
CRV_STATIC_ASSERT(CRV_EV_REL == EV_REL, "EV_REL mismatch");
CRV_STATIC_ASSERT(CRV_SYN_REPORT == SYN_REPORT, "SYN_REPORT mismatch");
CRV_STATIC_ASSERT(CRV_REL_X == REL_X, "REL_X mismatch");
CRV_STATIC_ASSERT(CRV_REL_Y == REL_Y, "REL_Y mismatch");
