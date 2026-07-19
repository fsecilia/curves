// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief input capture abi
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

typedef __UINT16_TYPE__ crv_capture_u16_t;
typedef __UINT32_TYPE__ crv_capture_u32_t;
typedef __UINT64_TYPE__ crv_capture_u64_t;

static crv_capture_u64_t const CRV_CAPTURE_STREAM_MAGIC = 0x3130504143565243ULL; // "CRVCAP01" in little-endian bytes

enum
{
    CRV_CAPTURE_ABI_MAJOR = 1,
    CRV_CAPTURE_ABI_MINOR = 0,

    CRV_CAPTURE_STREAM_RAW_INPUT_VALUES = 1,

    CRV_CAPTURE_TIMESTAMP_CALLBACK_MONOTONIC_NS = 1,
};

struct crv_capture_stream_header_t
{
    crv_capture_u64_t magic;

    crv_capture_u16_t abi_major;
    crv_capture_u16_t abi_minor;

    crv_capture_u16_t stream_kind;
    crv_capture_u16_t timestamp_kind;

    crv_capture_u32_t header_size;
    crv_capture_u32_t batch_header_size;
    crv_capture_u32_t input_value_size;
    crv_capture_u32_t flags;
};

struct crv_capture_batch_header_t
{
    crv_capture_u64_t timestamp_ns;
    crv_capture_u64_t sequence;

    crv_capture_u32_t count;
    crv_capture_u32_t capacity;
};
