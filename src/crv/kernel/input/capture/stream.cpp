// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "stream.hpp"

// extern "C" {

extern "C" int crv_capture_producer_required_scratch_size(crv_capture_size_t capacity, crv_capture_size_t* result)
{
    if (result == nullptr) return false;

    constexpr auto wire_capacity_max = crv::max<crv_capture_u32_t>();
    if (capacity > wire_capacity_max) return false;

    auto const size = crv::capture_batch_size(capacity);
    if (!size) return false;

    *result = *size;
    return true;
}

// } // extern "C" {
