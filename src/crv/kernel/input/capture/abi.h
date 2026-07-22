// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief binary abi for raw linux input-value capture stream
///
/// Capture streams are always in native byte order. The header contains a marker to determine byte order.
///
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

/// device, handle, and handler name; also the /dev node name
#define CRV_CAPTURE_NAME "crv-input-capture"

typedef __INT32_TYPE__ crv_capture_s32_t;
typedef __UINT8_TYPE__ crv_capture_u8_t;
typedef __UINT16_TYPE__ crv_capture_u16_t;
typedef __UINT32_TYPE__ crv_capture_u32_t;
typedef __UINT64_TYPE__ crv_capture_u64_t;

#define CRV_CAPTURE_FORMAT_VERSION 1u
#define CRV_CAPTURE_STREAM_MAGIC "CRVINP1"

/// marker identifying the native byte order used by this mvp format
#define CRV_CAPTURE_BYTE_ORDER_MARKER 0x01020304u

enum crv_capture_clock_id_t
{
    CRV_CAPTURE_CLOCK_MONOTONIC = 1,
};

enum crv_capture_frame_type_t
{
    CRV_CAPTURE_FRAME_TYPE_INPUT_VALUES = 1,
};

/// header for entire stream
struct crv_capture_stream_header_t
{
    crv_capture_u8_t magic[8];

    crv_capture_u32_t format_version;
    crv_capture_u32_t header_size;
    crv_capture_u32_t input_value_size;
    crv_capture_u32_t clock_id;
    crv_capture_u32_t byte_order_marker;
    crv_capture_u32_t flags;

    crv_capture_u64_t reserved[4];
};

/// header for generic frame
struct crv_capture_frame_header_t
{
    crv_capture_u32_t frame_size;
    crv_capture_u16_t frame_type;
    crv_capture_u16_t header_size;
};

/// frame for input_values array
struct crv_capture_input_values_header_t
{
    struct crv_capture_frame_header_t frame;

    crv_capture_u64_t timestamp_ns;
    crv_capture_u64_t sequence;

    crv_capture_u32_t value_count;
    crv_capture_u32_t value_capacity;

    // Following this frame are `value_count` instances of `crv_input_value_t` records.
    // The final frame in a capture may be truncated; truncated frames should be discarded.
};
