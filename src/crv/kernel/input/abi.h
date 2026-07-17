// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief common linux input handler abi
///
/// C++ code can't include the necessary Linux headers directly, so components are redeclared here compatibly, then
/// asserted in abi.c for compatibility.
///
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

typedef __INT32_TYPE__ crv_input_s32_t;
typedef __INT64_TYPE__ crv_input_s64_t;
typedef __UINT16_TYPE__ crv_input_u16_t;
typedef __UINT64_TYPE__ crv_input_u64_t;

struct crv_input_value_t
{
    crv_input_u16_t type;
    crv_input_u16_t code;
    crv_input_s32_t value;
};

enum
{
    CRV_EV_SYN = 0x00,
    CRV_EV_REL = 0x02,

    CRV_SYN_REPORT = 0x00,

    CRV_REL_X = 0x00,
    CRV_REL_Y = 0x01,
};
