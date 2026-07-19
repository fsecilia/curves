// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

extern "C" {
#include <crv/kernel/input/capture/abi.h>
} // extern "C"

#include <crv/lib.hpp>
#include <array>
#include <bit>

namespace crv {
namespace {

constexpr auto expected_magic_bytes = std::array{
    std::byte{'C'},
    std::byte{'R'},
    std::byte{'V'},
    std::byte{'C'},
    std::byte{'A'},
    std::byte{'P'},
    std::byte{'0'},
    std::byte{'1'},
};

static_assert(std::bit_cast<std::array<std::byte, sizeof(CRV_CAPTURE_STREAM_MAGIC)>>(CRV_CAPTURE_STREAM_MAGIC)
    == expected_magic_bytes);

static_assert(sizeof(crv_capture_stream_header_t) == 32);
static_assert(sizeof(crv_capture_batch_header_t) == 24);

} // namespace
} // namespace crv
