// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "fifo.h"
#include <linux/kfifo.h>

struct crv_fifo_t
{
    DECLARE_KFIFO(fifo, struct crv_capture_event_t, CRV_CAPTURE_FIFO_EVENT_COUNT);
};
static struct crv_fifo_t instance;

crv_fifo_handle_t crv_fifo_instance(void)
{
    return &instance;
}

void crv_fifo_init(crv_fifo_handle_t handle)
{
    INIT_KFIFO(handle->fifo);
}

void crv_fifo_reset(crv_fifo_handle_t handle)
{
    kfifo_reset(&handle->fifo);
}

bool crv_fifo_is_empty(crv_fifo_handle_t handle)
{
    return kfifo_is_empty(&handle->fifo);
}

unsigned int crv_fifo_avail(crv_fifo_handle_t handle)
{
    return kfifo_avail(&handle->fifo);
}

bool crv_fifo_put(crv_fifo_handle_t handle, const struct crv_capture_event_t* events)
{
    return kfifo_put(&handle->fifo, *events) == 1;
}

int crv_fifo_to_user(crv_fifo_handle_t handle, char __user* dst, unsigned int len, unsigned int* copied)
{
    return kfifo_to_user(&handle->fifo, dst, len, copied);
}
