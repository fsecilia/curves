// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief linux ioctl definitions for the userspace control abi
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/kernel/control/abi.h>
#include <linux/ioctl.h>

// provisional out-of-tree type; not reserved in the upstream ioctl registry
#define CRV_CONTROL_IOCTL_TYPE 0xC9

#define CRV_CONTROL_IOCTL_GET_DEVICE_V1 _IOWR(CRV_CONTROL_IOCTL_TYPE, 0x00, struct crv_control_device_v1_t)
#define CRV_CONTROL_IOCTL_APPLY_V1 _IOW(CRV_CONTROL_IOCTL_TYPE, 0x01, struct crv_control_apply_v1_t)
