// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/kernel/input/abi.h>
#include <crv/kernel/input/capture/abi.h>

typedef __SIZE_TYPE__ crv_capture_size_t;

/// opaque byte sink implemented by the linux c layer
struct crv_capture_byte_sink_t;

/// appends all bytes or appends nothing
///
/// \returns true iff the complete byte range was appended.
int crv_capture_byte_sink_try_write_exact(
    struct crv_capture_byte_sink_t* sink, void const* source, crv_capture_size_t size);

struct crv_capture_producer_state_t
{
    crv_capture_u64_t next_sequence;
    crv_capture_u64_t batches_written;
    crv_capture_u64_t batches_dropped;
    crv_capture_u64_t bytes_written;
};

struct crv_capture_producer_context_t
{
    struct crv_capture_byte_sink_t* sink;

    void* scratch;
    crv_capture_size_t scratch_size;

    struct crv_capture_producer_state_t state;
};

enum crv_capture_push_result_t
{
    CRV_CAPTURE_PUSHED,
    CRV_CAPTURE_QUEUE_FULL,
    CRV_CAPTURE_INVALID_INPUT,
    CRV_CAPTURE_SCRATCH_TOO_SMALL,
};

/// resets producer state and appends the authoritative stream header
///
/// The sink and scratch storage must already be initialized.
int crv_capture_producer_begin_session(struct crv_capture_producer_context_t* context);

/// frames and attempts to append one complete input callback
enum crv_capture_push_result_t crv_capture_producer_try_push(struct crv_capture_producer_context_t* context,
    crv_capture_u64_t timestamp_ns, struct crv_input_value_t const* values, crv_capture_size_t count,
    crv_capture_size_t capacity);
