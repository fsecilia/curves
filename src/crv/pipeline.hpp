// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/pipeline/input_frame.hpp>
#include <crv/pipeline/filters/half_life_ema.hpp>
#include <crv/pipeline/orchestrator.hpp>
#include <crv/pipeline/output_transform.hpp>
#include <crv/pipeline/relative_report.hpp>
#include <crv/pipeline/report_timer.hpp>
#include <crv/pipeline/residual_accumulator.hpp>
#include <crv/pipeline/velocity.hpp>
#include <crv/prefetcher.hpp>
#include <crv/spline/pipeline_config.hpp>
#include <crv/spline/segment.hpp>
#include <crv/spline/segment_locator.hpp>
#include <crv/spline/spline.hpp>
#include <crv/spline/tangent_extension.hpp>
#include <cstddef>
#include <utility>

namespace crv {

/// production runtime mouse pipeline
class pipeline_t
{
    using spline_config_t = spline::prod_pipeline_config_t;
    using speed_t = spline_config_t::x_t;
    using gain_value_t = spline_config_t::y_t;

    using unpacked_field_t = spline::unpacked_field_t<int_t>;
    using spline_traits_t = spline::traits_t<unpacked_field_t, gain_value_t>;
    using spline_segment_evaluator_t = spline::segment_evaluator_t<spline_traits_t, speed_t, gain_value_t>;
    using spline_field_unpacker_t = spline::field_unpacker_t<unpacked_field_t>;
    using spline_segment_unpacker_t = spline::segment_unpacker_t<spline_traits_t::packed_segment_t,
        spline_traits_t::unpacked_segment_t, spline_field_unpacker_t, spline_config_t::segment_layout>;
    using spline_segment_t
        = spline::segment_t<spline_traits_t, speed_t, spline_segment_unpacker_t, spline_segment_evaluator_t>;
    static constexpr auto spline_depth_max = 4;
    using spline_segment_locator_t = spline::segment_locator_t<speed_t, spline_depth_max>;
    using spline_extended_tangent_t = spline::extended_tangent_t<speed_t, gain_value_t, unpacked_field_t>;
    using gain_impl_t = spline::spline_t<spline_segment_t, spline_extended_tangent_t, spline_segment_locator_t>;

    using magnitude_rsqrt_t = rsqrt_t<fixed_t<uint64_t, 62>, fixed_t<uint64_t, 0>>;
    using magnitude_t = pipeline::displacement_magnitude_t<magnitude_rsqrt_t>;
    using velocity_impl_t = pipeline::velocity_t<speed_t, magnitude_t>;
    using duration_impl_t = velocity_impl_t::duration_t;
    using speed_filter_t = pipeline::half_life_ema_t<speed_t, duration_impl_t>;
    using output_transform_impl_t = pipeline::output_transform_t<gain_value_t>;
    using accumulator_t = pipeline::residual_accumulator_t<output_transform_impl_t::out_t>;
    using timer_t = pipeline::report_timer_t<duration_impl_t>;
    using orchestrator_t = pipeline::orchestrator_t<timer_t, velocity_impl_t, speed_filter_t, gain_impl_t,
        output_transform_impl_t, accumulator_t, static_prefetcher_t>;

    static_assert(sizeof(gain_impl_t::hint_t) == 24);
    static_assert(sizeof(orchestrator_t::config_t) == 64);
    static_assert(sizeof(orchestrator_t::state_t) == 64);

public:
    using config_t = orchestrator_t::config_t;
    using duration_t = orchestrator_t::duration_t;
    using timestamp_t = orchestrator_t::timestamp_t;
    using velocity_scale_t = orchestrator_t::velocity_scale_t;
    using gain_t = gain_impl_t;

    constexpr pipeline_t() noexcept = default;
    constexpr pipeline_t(config_t config, gain_t const& gain) noexcept
        : orchestrator_{.config = std::move(config), .gain = gain}
    {}

    struct result_t
    {
        pipeline::pipeline_result_t status;
        std::size_t count;
    };

    auto operator()(void* values, std::size_t count, std::size_t max_vals, std::size_t num_vals, timestamp_t timestamp)
        noexcept -> result_t
    {
        orchestrator_.prefetch();

        if (!synchronized_) [[unlikely]]
        {
            if (!input_core_forced_split(num_vals, max_vals))
            {
                orchestrator_.state = {};
                (void)orchestrator_.state.timer(timestamp);
                synchronized_ = true;
            }

            return {
                .status = count > max_vals ? pipeline::pipeline_result_t::invalid_report
                                           : pipeline::pipeline_result_t::split_report_bypassed,
                .count = count,
            };
        }

        if (input_core_forced_split(num_vals, max_vals)) [[unlikely]]
        {
            synchronized_ = false;
            return {
                .status = count > max_vals ? pipeline::pipeline_result_t::invalid_report
                                           : pipeline::pipeline_result_t::split_report_bypassed,
                .count = count,
            };
        }

        auto adapter = input_value_array_adapter_t{values, max_vals};
        auto frame = pipeline::input_frame_t{adapter, count};
        auto report = pipeline::relative_report_t{frame};

        return {
            .status = orchestrator_.process(report, timestamp),
            .count = frame.count(),
        };
    }

private:
    static constexpr auto input_core_forced_split(std::size_t num_vals, std::size_t max_vals) noexcept -> bool
    {
        // supported input-core baseline appends SYN_REPORT after reaching max_vals - 2
        return max_vals > 1 && num_vals >= max_vals - 1;
    }

    orchestrator_t orchestrator_{};
    bool synchronized_ = true;
};

} // namespace crv
