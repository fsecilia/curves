// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief captures input_value array to streamable node
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

struct input_handle;
struct input_value;

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
void crv_capture_record(struct input_handle* handle, const struct input_value* values, unsigned int count);

/// detaches input handle
///
/// If the handle is the active source, an open stream terminates with ENODEV after all previously buffered bytes have
/// been drained.
void crv_capture_detach(struct input_handle* handle);

/// unregisters the userspace capture-stream device
///
/// All sources must be detached before this function is called.
void crv_capture_unregister(void);
