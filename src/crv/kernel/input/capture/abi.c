// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "abi.h"
#include <linux/compiler.h>
#include <linux/input.h>
#include <linux/stddef.h>

//
// fundamental types
//

_Static_assert(sizeof(crv_s32_t) == sizeof(s32), "crv_input_s32_t size mismatch");
_Static_assert(sizeof(crv_u8_t) == sizeof(u8), "crv_input_u8_t size mismatch");
_Static_assert(sizeof(crv_u16_t) == sizeof(u16), "crv_input_u16_t size mismatch");
_Static_assert(sizeof(crv_u32_t) == sizeof(u32), "crv_input_u32_t size mismatch");
_Static_assert(sizeof(crv_u64_t) == sizeof(u64), "crv_input_u64_t size mismatch");

#define CRV_IS_SIGNED_TYPE(type) (((type) - 1) < (type)1)
_Static_assert(CRV_IS_SIGNED_TYPE(crv_s32_t) == CRV_IS_SIGNED_TYPE(s32), "crv_capture_s32_t sign mismatch");
_Static_assert(CRV_IS_SIGNED_TYPE(crv_u8_t) == CRV_IS_SIGNED_TYPE(u8), "crv_capture_u8_t sign mismatch");
_Static_assert(CRV_IS_SIGNED_TYPE(crv_u16_t) == CRV_IS_SIGNED_TYPE(u16), "crv_capture_u16_t sign mismatch");
_Static_assert(CRV_IS_SIGNED_TYPE(crv_u32_t) == CRV_IS_SIGNED_TYPE(u32), "crv_capture_u32_t sign mismatch");
_Static_assert(CRV_IS_SIGNED_TYPE(crv_u64_t) == CRV_IS_SIGNED_TYPE(u64), "crv_capture_u64_t sign mismatch");

//
// crv_capture_stream_header_t
//

_Static_assert(sizeof(CRV_CAPTURE_STREAM_MAGIC) == 8, "crv_capture_stream_header_t: magic must be eight bytes");
_Static_assert(sizeof(struct crv_capture_stream_header_t) == 64, "crv_capture_stream_header_t: unexpected layout");
CRV_VALIDATE_FIELD(crv_capture_stream_header_t, magic, 8, 0);
CRV_VALIDATE_FIELD(crv_capture_stream_header_t, format_version, 4, 8);
CRV_VALIDATE_FIELD(crv_capture_stream_header_t, header_size, 4, 12);
CRV_VALIDATE_FIELD(crv_capture_stream_header_t, input_value_size, 4, 16);
CRV_VALIDATE_FIELD(crv_capture_stream_header_t, clock_id, 4, 20);
CRV_VALIDATE_FIELD(crv_capture_stream_header_t, byte_order_marker, 4, 24);
CRV_VALIDATE_FIELD(crv_capture_stream_header_t, flags, 4, 28);

//
// crv_capture_frame_header_t
//

_Static_assert(sizeof(struct crv_capture_frame_header_t) == 8, "crv_capture_frame_header_t: unexpected layout");
CRV_VALIDATE_FIELD(crv_capture_frame_header_t, frame_size, 4, 0);
CRV_VALIDATE_FIELD(crv_capture_frame_header_t, frame_type, 2, 4);
CRV_VALIDATE_FIELD(crv_capture_frame_header_t, header_size, 2, 6);

//
// crv_capture_callback_header_t
//

_Static_assert(
    sizeof(struct crv_capture_input_values_header_t) == 32, "crv_capture_input_values_header_t: unexpected layout");
CRV_VALIDATE_FIELD(crv_capture_input_values_header_t, frame, 8, 0);
CRV_VALIDATE_FIELD(crv_capture_input_values_header_t, timestamp_ns, 8, 8);
CRV_VALIDATE_FIELD(crv_capture_input_values_header_t, sequence, 8, 16);
CRV_VALIDATE_FIELD(crv_capture_input_values_header_t, value_count, 4, 24);
CRV_VALIDATE_FIELD(crv_capture_input_values_header_t, value_capacity, 4, 28);
