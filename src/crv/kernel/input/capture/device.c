// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "device.h"
#include <linux/kfifo.h>
#include <linux/mutex.h>
#include <linux/stddef.h>
#include <linux/wait.h>

struct crv_capture_byte_sink_t
{
    struct kfifo fifo;
};

struct crv_capture_session_t
{
    struct crv_capture_byte_sink_t sink;
    struct crv_capture_producer_context_t producer;

    wait_queue_head_t readable;
    struct mutex read_mutex;

    void* scratch;
    size_t scratch_size;
};

int crv_capture_byte_sink_try_write_exact(
    struct crv_capture_byte_sink_t* sink, void const* source, crv_capture_size_t size)
{
    if (sink == NULL) return 0;
    if (size == 0) return 1;
    if (source == NULL) return 0;

    if (kfifo_avail(&sink->fifo) < size) return false;

    return kfifo_in(&sink->fifo, source, size) == size;
}
