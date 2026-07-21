// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief shared kfifo definitions
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/kernel/input/capture/abi.h>

#if !defined __KERNEL__
#define __user
#endif

/// number of crv_capture_event objects retained in the stream FIFO
///
/// At 24 bytes per value this occupies 1.5 MiB of module BSS. For a mouse producing four input values at 4 kHz, it
/// holds approximately four seconds.
#define CRV_CAPTURE_FIFO_EVENT_COUNT 65536u

struct crv_fifo_t;
typedef struct crv_fifo_t* crv_fifo_handle_t;

crv_fifo_handle_t crv_fifo_instance(void);
void crv_fifo_init(crv_fifo_handle_t);
void crv_fifo_reset(crv_fifo_handle_t);
bool crv_fifo_is_empty(crv_fifo_handle_t);
unsigned int crv_fifo_avail(crv_fifo_handle_t);
bool crv_fifo_put(crv_fifo_handle_t, const struct crv_capture_event_t*);
int crv_fifo_to_user(crv_fifo_handle_t handle, char __user* dst, unsigned int len, unsigned int* copied);
