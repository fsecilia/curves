// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief clean c header defining input_value analogs
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/kernel/abi.h>

struct crv_input_value_t
{
    crv_u16_t type;
    crv_u16_t code;
    crv_s32_t value;
};

enum
{
    CRV_EV_SYN = 0x00,
    CRV_EV_REL = 0x02,
};

enum
{
    CRV_SYN_REPORT = 0x00,
};

enum
{
    CRV_REL_X = 0x00,
    CRV_REL_Y = 0x01,
};
