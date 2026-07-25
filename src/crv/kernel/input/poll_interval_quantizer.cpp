// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

extern "C" {
#include "poll_interval_quantizer.h"
} // extern "C" {

#include <crv/lib.hpp>
#include <crv/filter/poll_interval_quantizer.hpp>
#include <crv/kernel/abi.h>
#include <crv/math/fixed/fixed.hpp>
#include <cassert>

using namespace crv;

extern "C" {

void crv_log_timestamp_regression(
    crv_u64_t previous_timestamp, crv_u64_t observed_timestamp, crv_u64_t repaired_timestamp);

crv_u64_t crv_quantize_timestamp(crv_u64_t timestamp)
{
    using quantizer_t = poll_interval_quantizer_t;
    using timestamp_t = quantizer_t::timestamp_t;
    using status_t = quantizer_t::status_t;

    static auto poll_interval_quantizer = quantizer_t{quantizer_t::period_t{1'000'000'000ULL / 4000}};

    auto const had_previous = poll_interval_quantizer.initialized();
    auto const previous_timestamp = had_previous ? poll_interval_quantizer.previous_timestamp().value : timestamp;
    auto const result = poll_interval_quantizer.observe(timestamp_t::literal(timestamp));
    auto const quantized_timestamp = result.quantized_timestamp().value;

    if (result.status == status_t::timestamp_regressed)
    {
        crv_log_timestamp_regression(previous_timestamp, timestamp, quantized_timestamp);
    }

    return quantized_timestamp;
}

} // extern "C"
