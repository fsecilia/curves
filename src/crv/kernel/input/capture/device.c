// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "device.h"
#include <linux/kfifo.h>

struct crv_capture_byte_sink_t
{
    struct kfifo fifo;
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
