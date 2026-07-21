// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief binary abi for raw linux input-value capture stream
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#ifdef __KERNEL__
#include <linux/stddef.h>
#include <linux/types.h>

typedef __u8 crv_capture_u8;
typedef __u16 crv_capture_u16;
typedef __u32 crv_capture_u32;
typedef __u64 crv_capture_u64;
typedef __s32 crv_capture_s32;
#else
#include <stddef.h>
#include <stdint.h>

typedef uint8_t crv_capture_u8;
typedef uint16_t crv_capture_u16;
typedef uint32_t crv_capture_u32;
typedef uint64_t crv_capture_u64;
typedef int32_t crv_capture_s32;
#endif

/// abi version written into crv_capture_file_header
#define CRV_CAPTURE_ABI_VERSION 1u

/// 8 byte file magic, including the terminating zero
#define CRV_CAPTURE_FILE_MAGIC "CRVINP1"

/// marker identifying the native byte order used by this mvp format
#define CRV_CAPTURE_BYTE_ORDER_MARKER 0x01020304u

enum crv_capture_clock_id_t
{
    CRV_CAPTURE_CLOCK_MONOTONIC = 1,
};

/// header written by user-mode capture program before stream records
///
/// The mvp file format uses the host's native byte order. A reader can compare byte_order_marker against
/// CRV_CAPTURE_BYTE_ORDER_MARKER and byte-swap the remaining integer fields when necessary.
struct crv_capture_file_header_t
{
    crv_capture_u8 magic[8];
    crv_capture_u32 abi_version;
    crv_capture_u32 header_size;
    crv_capture_u32 record_size;
    crv_capture_u32 clock_id;
    crv_capture_u32 byte_order_marker;
    crv_capture_u32 reserved;
};

/// one value from one input-handler events() callback
///
/// Every value from the same callback has the same timestamp_ns and batch_sequence. A gap in batch_sequence means at
/// least one whole callback was dropped because the kernel FIFO lacked room for all of its values.
struct crv_capture_event_t
{
    crv_capture_u64 timestamp_ns;
    crv_capture_u64 batch_sequence;
    crv_capture_u16 type;
    crv_capture_u16 code;
    crv_capture_s32 value;
};

#if defined(__cplusplus)
static_assert(sizeof(CRV_CAPTURE_FILE_MAGIC) == 8);
static_assert(sizeof(struct crv_capture_file_header_t) == 32);
static_assert(offsetof(struct crv_capture_file_header_t, abi_version) == 8);
static_assert(offsetof(struct crv_capture_file_header_t, byte_order_marker) == 24);
static_assert(sizeof(struct crv_capture_event_t) == 24);
static_assert(offsetof(struct crv_capture_event_t, timestamp_ns) == 0);
static_assert(offsetof(struct crv_capture_event_t, batch_sequence) == 8);
static_assert(offsetof(struct crv_capture_event_t, type) == 16);
static_assert(offsetof(struct crv_capture_event_t, code) == 18);
static_assert(offsetof(struct crv_capture_event_t, value) == 20);
#else
_Static_assert(sizeof(CRV_CAPTURE_FILE_MAGIC) == 8, "capture magic must be eight bytes");
_Static_assert(sizeof(struct crv_capture_file_header_t) == 32, "unexpected capture file header layout");
_Static_assert(offsetof(struct crv_capture_file_header_t, abi_version) == 8, "unexpected ABI version offset");
_Static_assert(offsetof(struct crv_capture_file_header_t, byte_order_marker) == 24, "unexpected byte-order offset");
_Static_assert(sizeof(struct crv_capture_event_t) == 24, "unexpected capture event layout");
_Static_assert(offsetof(struct crv_capture_event_t, timestamp_ns) == 0, "unexpected timestamp offset");
_Static_assert(offsetof(struct crv_capture_event_t, batch_sequence) == 8, "unexpected sequence offset");
_Static_assert(offsetof(struct crv_capture_event_t, type) == 16, "unexpected type offset");
_Static_assert(offsetof(struct crv_capture_event_t, code) == 18, "unexpected code offset");
_Static_assert(offsetof(struct crv_capture_event_t, value) == 20, "unexpected value offset");
#endif
