// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief clean c header defining pipeline result abi
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/kernel/abi.h>

enum
{
    CRV_PIPELINE_RESULT_APPLIED = 0,
    CRV_PIPELINE_RESULT_WARMUP = 1,
    CRV_PIPELINE_RESULT_INVALID_REPORT = 2,
    CRV_PIPELINE_RESULT_INVALID_TIMESTAMP = 3,
    CRV_PIPELINE_RESULT_VELOCITY_OUT_OF_RANGE = 4,
    CRV_PIPELINE_RESULT_TRANSFORM_INPUT_OUT_OF_RANGE = 5,
    CRV_PIPELINE_RESULT_OUTPUT_OUT_OF_RANGE = 6,
    CRV_PIPELINE_RESULT_APPEND_FAILED = 7,
    CRV_PIPELINE_RESULT_SPLIT_REPORT_BYPASSED = 8,
    CRV_PIPELINE_RESULT_INACTIVE = 9,
};

struct crv_pipeline_result_t
{
    crv_u32_t status;
    crv_u32_t count;
};
