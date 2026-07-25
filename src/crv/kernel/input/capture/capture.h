// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief captures input_value array to streamable node
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/kernel/abi.h>

struct input_handle;
struct input_value;

void crv_log_timestamp_regression(
    crv_u64_t previous_timestamp, crv_u64_t observed_timestamp, crv_u64_t repaired_timestamp);

/// registers userspace capture-stream device
///
/// The capture subsystem must be registered before a source is attached.
int crv_capture_register(void);

/// attaches input handle whose callbacks may be captured
///
/// The input handle remains owned by the caller and must remain alive until crv_capture_detach() returns.
///
/// \returns -EBUSY if another source is already attached.
int crv_capture_attach(struct input_handle* handle);

/// records one input-handler callback
///
/// Capture is observational. This function neither modifies the values nor determines how many values the input handler
/// forwards.
void crv_capture_record(
    struct input_handle* handle, const struct input_value* values, unsigned int count, crv_u64_t timestamp_ns);

/// detaches input handle
///
/// If the handle is the active source, an open stream terminates with ENODEV after all previously buffered bytes have
/// been drained.
void crv_capture_detach(struct input_handle* handle);

/// unregisters the userspace capture-stream device
///
/// All sources must be detached before this function is called.
void crv_capture_unregister(void);
