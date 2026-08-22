// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/pipeline/status.hpp>
#include <utility>

namespace crv::pipeline {

/// runtime mouse pipeline with injected stages
///
/// Orders stages, prefetches their data, and commits residual state after the report update succeeds.
template <typename t_timer_t, typename t_velocity_t, typename t_speed_filter_t, typename t_gain_t,
    typename t_output_transform_t, typename t_accumulator_t, typename t_prefetcher_t>
class orchestrator_t
{
public:
    using timer_t = t_timer_t;
    using velocity_t = t_velocity_t;
    using speed_filter_t = t_speed_filter_t;
    using gain_t = t_gain_t;
    using output_transform_t = t_output_transform_t;
    using accumulator_t = t_accumulator_t;
    using prefetcher_t = t_prefetcher_t;

    using duration_t = typename timer_t::duration_t;
    using timestamp_t = typename timer_t::timestamp_t;
    using velocity_scale_t = typename velocity_t::scale_t;

    struct alignas(64) config_t
    {
        velocity_scale_t velocity_scale{};
        duration_t half_life{};
        output_transform_t output_transform{};
    };

    struct alignas(64) state_t
    {
        timer_t timer{};
        speed_filter_t speed_filter{};
        accumulator_t accumulator{};
    };

    config_t config{};
    state_t state{};
    gain_t gain{};

    [[no_unique_address]] velocity_t velocity{};
    [[no_unique_address]] prefetcher_t prefetcher{};

    auto prefetch() const noexcept -> void
    {
        prefetcher.prefetch(&config);
        prefetcher.prefetch(&state);
    }

    template <typename report_t> auto process(report_t& report, timestamp_t timestamp) noexcept -> pipeline_result_t
    {
        if (!report.valid()) return pipeline_result_t::invalid_report;

        auto const timing = state.timer(timestamp);
        if (timing.status == timer_t::status_t::initial) return pipeline_result_t::warmup;
        if (timing.status != timer_t::status_t::ready) return pipeline_result_t::invalid_timestamp;

        gain.prefetch(prefetcher);

        auto const speed = velocity(report.x(), report.y(), timing.duration, config.velocity_scale);
        if (!speed.valid) return pipeline_result_t::velocity_out_of_range;

        auto const filtered_speed = state.speed_filter(speed.value, config.half_life, timing.duration);
        auto const scalar_gain = gain(filtered_speed);
        auto const transformed = config.output_transform(report.x(), report.y(), scalar_gain);
        if (!transformed.valid) return pipeline_result_t::transform_input_out_of_range;

        auto const reservation = state.accumulator.reserve(transformed.x, transformed.y);
        if (!reservation.valid) return pipeline_result_t::output_out_of_range;
        if (!std::move(report).try_store(reservation.x, reservation.y)) return pipeline_result_t::append_failed;

        state.accumulator.commit(reservation);
        return pipeline_result_t::applied;
    }
};

} // namespace crv::pipeline
