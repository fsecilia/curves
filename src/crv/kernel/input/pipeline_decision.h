// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief pipeline result policy for linux input callbacks
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/kernel/pipeline/abi.h>

#ifdef __cplusplus
extern "C" {
#endif

enum crv_input_pipeline_diagnostic_t
{
    CRV_INPUT_PIPELINE_DIAGNOSTIC_NONE = 0,
    CRV_INPUT_PIPELINE_DIAGNOSTIC_SPLIT_REPORT = 1,
    CRV_INPUT_PIPELINE_DIAGNOSTIC_INVALID_REPORT = 2,
    CRV_INPUT_PIPELINE_DIAGNOSTIC_INVALID_COUNT = 3,
    CRV_INPUT_PIPELINE_DIAGNOSTIC_INVALID_TIMESTAMP = 4,
    CRV_INPUT_PIPELINE_DIAGNOSTIC_VELOCITY_OUT_OF_RANGE = 5,
    CRV_INPUT_PIPELINE_DIAGNOSTIC_TRANSFORM_INPUT_OUT_OF_RANGE = 6,
    CRV_INPUT_PIPELINE_DIAGNOSTIC_OUTPUT_OUT_OF_RANGE = 7,
    CRV_INPUT_PIPELINE_DIAGNOSTIC_APPEND_FAILED = 8,
    CRV_INPUT_PIPELINE_DIAGNOSTIC_UNKNOWN_STATUS = 9,
    CRV_INPUT_PIPELINE_DIAGNOSTIC_IMPOSSIBLE_COUNT = 10,
};

struct crv_input_pipeline_decision_t
{
    crv_u32_t count;
    crv_u32_t diagnostic;
};

static inline struct crv_input_pipeline_decision_t crv_input_decide_pipeline_result(
    struct crv_pipeline_result_t result, crv_u32_t input_count, crv_u32_t capacity)
{
    struct crv_input_pipeline_decision_t decision = {
        .count = result.count,
        .diagnostic = CRV_INPUT_PIPELINE_DIAGNOSTIC_NONE,
    };

    switch (result.status)
    {
        case CRV_PIPELINE_RESULT_APPLIED:
        case CRV_PIPELINE_RESULT_WARMUP:
        case CRV_PIPELINE_RESULT_INACTIVE: break;

        case CRV_PIPELINE_RESULT_SPLIT_REPORT_BYPASSED:
            decision.diagnostic = CRV_INPUT_PIPELINE_DIAGNOSTIC_SPLIT_REPORT;
            break;

        case CRV_PIPELINE_RESULT_INVALID_REPORT:
            if (input_count > capacity)
            {
                decision.count = 0;
                decision.diagnostic = CRV_INPUT_PIPELINE_DIAGNOSTIC_INVALID_COUNT;
                return decision;
            }

            decision.diagnostic = CRV_INPUT_PIPELINE_DIAGNOSTIC_INVALID_REPORT;
            break;

        case CRV_PIPELINE_RESULT_INVALID_TIMESTAMP:
            decision.diagnostic = CRV_INPUT_PIPELINE_DIAGNOSTIC_INVALID_TIMESTAMP;
            break;

        case CRV_PIPELINE_RESULT_VELOCITY_OUT_OF_RANGE:
            decision.diagnostic = CRV_INPUT_PIPELINE_DIAGNOSTIC_VELOCITY_OUT_OF_RANGE;
            break;

        case CRV_PIPELINE_RESULT_TRANSFORM_INPUT_OUT_OF_RANGE:
            decision.diagnostic = CRV_INPUT_PIPELINE_DIAGNOSTIC_TRANSFORM_INPUT_OUT_OF_RANGE;
            break;

        case CRV_PIPELINE_RESULT_OUTPUT_OUT_OF_RANGE:
            decision.diagnostic = CRV_INPUT_PIPELINE_DIAGNOSTIC_OUTPUT_OUT_OF_RANGE;
            break;

        case CRV_PIPELINE_RESULT_APPEND_FAILED:
            decision.diagnostic = CRV_INPUT_PIPELINE_DIAGNOSTIC_APPEND_FAILED;
            break;

        default:
            decision.count = input_count > capacity ? 0 : input_count;
            decision.diagnostic = CRV_INPUT_PIPELINE_DIAGNOSTIC_UNKNOWN_STATUS;
            return decision;
    }

    if (decision.count > capacity)
    {
        decision.count = 0;
        decision.diagnostic = CRV_INPUT_PIPELINE_DIAGNOSTIC_IMPOSSIBLE_COUNT;
    }

    return decision;
}

#ifdef __cplusplus
} // extern "C" {
#endif
