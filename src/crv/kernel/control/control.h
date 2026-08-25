// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief kernel control endpoint and input attachment registry
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/kernel/pipeline/pipeline.h>
#include <linux/input.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/types.h>

struct crv_control_attachment
{
    struct list_head node;
    u64 id;
    struct input_handle* input;
    struct crv_pipeline* pipeline;
};

struct crv_control
{
    struct miscdevice misc;
    struct mutex mutex;
    struct list_head attachments;
    u64 next_attachment_id;
};

int crv_control_register(struct crv_control* control);
void crv_control_unregister(struct crv_control* control);

int crv_control_attach(struct crv_control* control, struct crv_control_attachment* attachment,
    struct input_handle* input, struct crv_pipeline* pipeline);
void crv_control_detach(struct crv_control* control, struct crv_control_attachment* attachment);
