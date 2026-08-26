// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief c api for preparing and committing runtime configuration
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/kernel/abi.h>
#include <crv/kernel/pipeline/pipeline.h>

#ifdef __cplusplus
extern "C" {
#endif

struct crv_control_prepared_configuration_t;

struct crv_control_validation_result_t
{
    crv_u32_t error;
    crv_s64_t segment_index;
};

crv_u32_t crv_control_prepared_configuration_storage_size(void);

struct crv_control_prepared_configuration_t* crv_control_prepared_configuration_construct(
    void* storage, crv_u32_t mode);
void crv_control_prepared_configuration_destroy(struct crv_control_prepared_configuration_t* configuration);

void* crv_control_prepared_configuration_config(struct crv_control_prepared_configuration_t* configuration);
crv_u32_t crv_control_prepared_configuration_config_size(void);
void* crv_control_prepared_configuration_gain(struct crv_control_prepared_configuration_t* configuration);
crv_u32_t crv_control_prepared_configuration_gain_size(void);

struct crv_control_validation_result_t crv_control_prepared_configuration_validate(
    struct crv_control_prepared_configuration_t* configuration);
char const* crv_control_validation_error_name(crv_u32_t error);

void crv_control_prepared_configuration_commit(
    struct crv_control_prepared_configuration_t const* configuration, struct crv_pipeline_t* pipeline);

#ifdef __cplusplus
} // extern "C" {
#endif
