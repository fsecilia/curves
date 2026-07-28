// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

extern "C" {
#include "filter.h"
#include <crv/kernel/abi.h>
} // extern "C" {

#include <crv/lib.hpp>
#include <crv/filter/timing_recovery/poll_interval_quantizer.hpp>
#include <crv/filter/timing_recovery/poll_period_tracker.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <cassert>

using namespace crv;

extern "C" {

crv_u64_t crv_quantize_timestamp(crv_u64_t timestamp)
{
    using period_t = poll_period_tracker_t::period_t;
    using quantizer_t = poll_interval_quantizer_t;
    using tracker_t = poll_period_tracker_t;
    using timestamp_t = quantizer_t::timestamp_t;
    using quantizer_status_t = quantizer_t::status_t;

    static constexpr auto initial_period = quantizer_t::period_t{1'000'000'000ULL / 4000};

    static constexpr auto tracker_params = tracker_t::params_t{
        .initial_period = initial_period,
        .measurement_window_ticks = 4'000,

        // provisional policy; this should eventually be justified by traces

        // explicit chosen threshold
        .maximum_training_gap_ticks = 0,

        .gain_shift = 4,
        .maximum_measurement_error = period_t{0},
        .maximum_correction = period_t{0},
        .minimum_period = period_t{0},
        .maximum_period = period_t{0},
    };

    static auto quantizer = quantizer_t{};
    static auto tracker = tracker_t{tracker_params};

    auto const raw_timestamp = timestamp_t::literal(timestamp);
    auto const report_period = tracker.estimated_period();
    auto const had_previous = quantizer.initialized();
    auto const previous_timestamp = had_previous ? quantizer.previous_timestamp().value : timestamp;
    auto const interval = quantizer.observe(raw_timestamp, report_period);

    switch (interval.status)
    {
        case quantizer_status_t::initialized: tracker.reanchor(raw_timestamp); break;

        case quantizer_status_t::continuous:
        {
            auto const update = tracker.observe(raw_timestamp, interval.elapsed_ticks);

            // Do not silently discard rejected/reanchored states. For the development integration, log or count them
            // here.
            static_cast<void>(update);
            break;
        }

        case quantizer_status_t::timestamp_regressed:
            tracker.reanchor(raw_timestamp);

            crv_log_timestamp_regression(previous_timestamp, timestamp, interval.quantized_timestamp().value);
            break;
    }

    return interval.quantized_timestamp().value;
}

} // extern "C"
