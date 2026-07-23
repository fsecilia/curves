// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "abi.h"
#include <linux/compiler.h>
#include <linux/input.h>
#include <linux/stddef.h>

//
// crv_capture_stream_header_t
//

_Static_assert(sizeof(CRV_CAPTURE_STREAM_MAGIC) == 8, "crv_capture_stream_header_t: magic must be eight bytes");
_Static_assert(sizeof(struct crv_capture_stream_header_t) == 64, "crv_capture_stream_header_t: unexpected layout");
CRV_MEMBER_LAYOUT(struct crv_capture_stream_header_t, magic, 8, 0);
CRV_MEMBER_LAYOUT_INTEGER(struct crv_capture_stream_header_t, format_version, 4, 8, false);
CRV_MEMBER_LAYOUT_INTEGER(struct crv_capture_stream_header_t, header_size, 4, 12, false);
CRV_MEMBER_LAYOUT_INTEGER(struct crv_capture_stream_header_t, input_value_size, 4, 16, false);
CRV_MEMBER_LAYOUT_INTEGER(struct crv_capture_stream_header_t, clock_id, 4, 20, false);
CRV_MEMBER_LAYOUT_INTEGER(struct crv_capture_stream_header_t, byte_order_marker, 4, 24, false);
CRV_MEMBER_LAYOUT_INTEGER(struct crv_capture_stream_header_t, flags, 4, 28, false);

//
// crv_capture_frame_header_t
//

_Static_assert(sizeof(struct crv_capture_frame_header_t) == 8, "crv_capture_frame_header_t: unexpected layout");
CRV_MEMBER_LAYOUT_INTEGER(struct crv_capture_frame_header_t, frame_size, 4, 0, false);
CRV_MEMBER_LAYOUT_INTEGER(struct crv_capture_frame_header_t, frame_type, 2, 4, false);
CRV_MEMBER_LAYOUT_INTEGER(struct crv_capture_frame_header_t, header_size, 2, 6, false);

//
// crv_capture_callback_header_t
//

_Static_assert(
    sizeof(struct crv_capture_input_values_header_t) == 32, "crv_capture_input_values_header_t: unexpected layout");
CRV_MEMBER_LAYOUT(struct crv_capture_input_values_header_t, frame, 8, 0);
CRV_MEMBER_LAYOUT_INTEGER(struct crv_capture_input_values_header_t, timestamp_ns, 8, 8, false);
CRV_MEMBER_LAYOUT_INTEGER(struct crv_capture_input_values_header_t, sequence, 8, 16, false);
CRV_MEMBER_LAYOUT_INTEGER(struct crv_capture_input_values_header_t, value_count, 4, 24, false);
CRV_MEMBER_LAYOUT_INTEGER(struct crv_capture_input_values_header_t, value_capacity, 4, 28, false);
