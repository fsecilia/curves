// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief userspace control abi
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/kernel/abi.h>

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    CRV_CONTROL_APPLY_MODE_BYPASSED = 0,
    CRV_CONTROL_APPLY_MODE_ACTIVE = 1,
};

enum
{
    CRV_CONTROL_DEVICE_SYSNAME_SIZE = 32,
    CRV_CONTROL_GAIN_V1_WORD_COUNT = 1380,
    CRV_CONTROL_GAIN_V1_LOCATOR_WORD_OFFSET = 0,
    CRV_CONTROL_GAIN_V1_SEGMENTS_WORD_OFFSET = 352,
    CRV_CONTROL_GAIN_V1_TANGENT_WORD_OFFSET = 1376,
    CRV_CONTROL_GAIN_V1_TANGENT_WORD_COUNT = 4,
};

struct crv_control_device_v1_t
{
    crv_u64_t after_attachment_id;
    crv_u64_t attachment_id;
    crv_u16_t bustype;
    crv_u16_t vendor;
    crv_u16_t product;
    crv_u16_t version;
    char sysname[CRV_CONTROL_DEVICE_SYSNAME_SIZE];
};

struct crv_control_runtime_config_v1_t
{
    crv_u64_t velocity_scale;
    crv_u64_t half_life;
    crv_s64_t output_transform[4];
};

struct crv_control_gain_v1_t
{
    crv_u64_t words[CRV_CONTROL_GAIN_V1_WORD_COUNT];
};

struct crv_control_configuration_v1_t
{
    struct crv_control_runtime_config_v1_t config;
    struct crv_control_gain_v1_t gain;
};

struct crv_control_apply_v1_t
{
    crv_u64_t attachment_id;
    crv_u64_t configuration;
    crv_u32_t configuration_size;
    crv_u32_t mode;
    crv_u64_t reserved;
};

#ifdef __cplusplus
} // extern "C" {
#endif
