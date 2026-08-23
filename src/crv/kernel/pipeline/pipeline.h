// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief c api for the production runtime pipeline
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/kernel/abi.h>
#include <crv/kernel/pipeline/abi.h>

#ifdef __cplusplus
extern "C" {
#endif

struct crv_pipeline;

crv_u32_t crv_pipeline_storage_size(void);

struct crv_pipeline* crv_pipeline_construct(void* storage);
void crv_pipeline_destroy(struct crv_pipeline* pipeline);

struct crv_pipeline_result_t crv_pipeline_process(struct crv_pipeline* pipeline, void* values, crv_u32_t count,
    crv_u32_t max_vals, crv_u32_t num_vals, crv_u64_t timestamp);

#ifdef __cplusplus
} // extern "C" {
#endif
