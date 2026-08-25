// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "abi.h"
#include <crv/pipeline/status.hpp>

namespace crv::pipeline {
namespace {

static_assert(sizeof(pipeline_result_t) == sizeof(crv_u32_t));
static_assert(static_cast<crv_u32_t>(pipeline_result_t::applied) == CRV_PIPELINE_RESULT_APPLIED);
static_assert(static_cast<crv_u32_t>(pipeline_result_t::warmup) == CRV_PIPELINE_RESULT_WARMUP);
static_assert(static_cast<crv_u32_t>(pipeline_result_t::invalid_report) == CRV_PIPELINE_RESULT_INVALID_REPORT);
static_assert(static_cast<crv_u32_t>(pipeline_result_t::invalid_timestamp) == CRV_PIPELINE_RESULT_INVALID_TIMESTAMP);
static_assert(
    static_cast<crv_u32_t>(pipeline_result_t::velocity_out_of_range) == CRV_PIPELINE_RESULT_VELOCITY_OUT_OF_RANGE);
static_assert(static_cast<crv_u32_t>(pipeline_result_t::transform_input_out_of_range)
    == CRV_PIPELINE_RESULT_TRANSFORM_INPUT_OUT_OF_RANGE);
static_assert(
    static_cast<crv_u32_t>(pipeline_result_t::output_out_of_range) == CRV_PIPELINE_RESULT_OUTPUT_OUT_OF_RANGE);
static_assert(static_cast<crv_u32_t>(pipeline_result_t::append_failed) == CRV_PIPELINE_RESULT_APPEND_FAILED);
static_assert(
    static_cast<crv_u32_t>(pipeline_result_t::split_report_bypassed) == CRV_PIPELINE_RESULT_SPLIT_REPORT_BYPASSED);
static_assert(static_cast<crv_u32_t>(pipeline_result_t::inactive) == CRV_PIPELINE_RESULT_INACTIVE);

} // namespace
} // namespace crv::pipeline
