// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/uabs.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/math/shifter.hpp>
#include <cassert>

namespace crv::pipeline::filters {

/// Recovers a smooth report-clock timeline from timestamps attached to the final report of each delivered batch.
///
/// Implements the second-order DLL from Fons Adriaensen, "Using a DLL to Filter Time":
///
///     error = observed_time - predicted_time
///     next_time += phase_error_gain * error + estimated_period
///     estimated_period += period_error_gain * error
///
/// Earlier reports in a batch are prediction-only report-clock steps. The one timestamp observation is applied to the
/// final report. If the final report is more than gap_threshold_periods late, report-clock continuity is considered
/// lost: the batch is right-justified at the observation, phase is reacquired, and the learned period is retained.
class report_clock_t
{
public:
    /// Host timestamps, expressed as integral nanoseconds.
    using timestamp_t = fixed_t<uint64_t, 0>;

    /// Estimated nanoseconds per report.
    ///
    /// Q32.32 gives sub-nanosecond accumulation while covering periods up to almost 2^32 ns (~4.29 s), far beyond mouse
    /// report intervals.
    using period_t = fixed_t<uint64_t, 32>;

    /// Dimensionless DLL gains in [0, 1).
    using gain_t = fixed_t<uint64_t, 64>;

    /// High-resolution absolute time used only inside the clock mapper.
    ///
    /// The 128-bit value is an add/subtract accumulator; the update loop never multiplies an absolute timestamp.
    using recovered_time_t = fixed_t<uint128_t, period_t::frac_bits>;

    using timing_error_t = fixed_t<int64_t, period_t::frac_bits>;
    using report_count_t = uint32_t;

    struct params_t
    {
        period_t nominal_period;
        gain_t phase_error_gain;
        gain_t period_error_gain;
        period_t minimum_period;
        period_t maximum_period;
        report_count_t gap_threshold_periods;
    };

    /// Timing assigned to one delivered batch.
    struct batch_timing_t
    {
        recovered_time_t final_report_time;
        period_t report_period;
        bool follows_gap;

        /// Exact recovered time of report index within this batch.
        constexpr auto report_time(report_count_t index, report_count_t report_count) const noexcept -> recovered_time_t
        {
            assert(report_count != 0);
            assert(index < report_count);

            auto const reports_after = report_count - 1 - index;
            auto const offset = scale(report_period, reports_after);
            assert(final_report_time >= offset);
            return final_report_time - offset;
        }

        /// Recovered report timestamp rounded to the nearest integral ns.
        constexpr auto report_timestamp(report_count_t index, report_count_t report_count) const noexcept -> timestamp_t
        {
            static constexpr auto rne_shifter = shifter_t<rounding_modes::shr::nearest_even>{};
            return timestamp_t::template convert<rne_shifter>(report_time(index, report_count));
        }

        /// Timestamp for the collapsed zero-displacement event immediately one estimated report period before the first
        /// real report in this batch.
        constexpr auto preceding_zero_time(report_count_t report_count) const noexcept -> recovered_time_t
        {
            assert(report_count != 0);

            auto const offset = scale(report_period, report_count);
            assert(final_report_time >= offset);
            return final_report_time - offset;
        }

        constexpr auto preceding_zero_timestamp(report_count_t report_count) const noexcept -> timestamp_t
        {
            static constexpr auto rne_shifter = shifter_t<rounding_modes::shr::nearest_even>{};
            return timestamp_t::template convert<rne_shifter>(preceding_zero_time(report_count));
        }

    private:
        using report_count_fixed_t = fixed_t<report_count_t, 0>;

        static constexpr auto scale(period_t period, report_count_t report_count) noexcept -> recovered_time_t
        {
            auto const product = multiply(period, report_count_fixed_t::literal(report_count));
            return recovered_time_t::convert(product);
        }
    };

    /// Production defaults for a 0.5 Hz critically damped DLL, a two-period continuity threshold, and a +/-5% period
    /// safety envelope.
    ///
    /// The gain constants fold in B = 0.5 Hz and nanoseconds:
    ///
    ///     omega / ns = 2*pi*B*1e-9 = pi*1e-9
    ///     b / ns     = sqrt(2)*omega/ns
    ///     c          = omega^2
    static constexpr auto default_params(period_t nominal_period) noexcept -> params_t
    {
        assert(nominal_period > period_t{});

        // Q0.64 nearest-even encodings of pi*1e-9 and pi*sqrt(2)*1e-9.
        static constexpr auto omega_per_ns = gain_t::literal(57'952'155'665ULL);
        static constexpr auto phase_gain_per_ns = gain_t::literal(81'956'724'510ULL);
        static constexpr auto rne_shifter = shifter_t<rounding_modes::shr::nearest_even>{};

        auto const phase_error_gain = multiply<gain_t, rne_shifter>(phase_gain_per_ns, nominal_period);
        auto const omega = multiply<gain_t, rne_shifter>(omega_per_ns, nominal_period);
        auto const period_error_gain = multiply<gain_t, rne_shifter>(omega, omega);

        // Five percent in the same Q32.32 representation.
        auto const tolerance = period_t::literal(nominal_period.value / 20);

        return {
            .nominal_period = nominal_period,
            .phase_error_gain = phase_error_gain,
            .period_error_gain = period_error_gain,
            .minimum_period = nominal_period - tolerance,
            .maximum_period = nominal_period + tolerance,
            .gap_threshold_periods = 4,
        };
    }

    constexpr explicit report_clock_t(period_t nominal_period) noexcept : report_clock_t{default_params(nominal_period)}
    {}

    constexpr explicit report_clock_t(params_t params) noexcept
        : params_{params}, estimated_period_{params.nominal_period}
    {
        assert(params_.nominal_period > period_t{});
        assert(params_.minimum_period > period_t{});
        assert(params_.minimum_period <= params_.nominal_period);
        assert(params_.nominal_period <= params_.maximum_period);
        assert(params_.gap_threshold_periods != 0);

        // timing_error_t must hold the positive continuity threshold in raw
        // Q32.32 units. Mouse periods are many orders of magnitude below this.
        auto const gap_limit = scale_period(params_.maximum_period, params_.gap_threshold_periods);
        assert(gap_limit.value <= static_cast<uint128_t>(max<int64_t>()));
    }

    /// Discards phase and restores the nominal period.
    constexpr void reset() noexcept
    {
        next_report_time_ = {};
        estimated_period_ = params_.nominal_period;
        previous_observed_time_ = {};
        initialized_ = false;
    }

    /// Updates the report clock from a timestamp associated with the final report in a nonempty batch.
    ///
    /// The current batch always uses the period estimate that existed before this observation. Corrections apply
    /// prospectively, beginning with the next report, matching the source DLL recurrence.
    ///
    /// \pre report_count > 0
    /// \pre observed_batch_time is monotonic
    constexpr auto operator()(timestamp_t observed_batch_time, report_count_t report_count) noexcept -> batch_timing_t
    {
        assert(report_count != 0);

        auto const observed_time = recovered_time_t::convert(observed_batch_time);

        if (!initialized_) [[unlikely]]
        {
            initialized_ = true;
            previous_observed_time_ = observed_batch_time;
            next_report_time_ = observed_time + recovered_time_t::convert(estimated_period_);

            return {
                .final_report_time = observed_time,
                .report_period = estimated_period_,
                .follows_gap = true,
            };
        }

        assert(observed_batch_time >= previous_observed_time_);
        previous_observed_time_ = observed_batch_time;

        auto const batch_period = estimated_period_;
        auto const reports_before_final = report_count - 1;
        auto const predicted_final = next_report_time_ + scale_period(batch_period, reports_before_final);
        auto const gap_limit = scale_period(batch_period, params_.gap_threshold_periods);

        // Idle elision and unusably late observations have the same safe MVP behavior: right-justify the batch,
        // preserve the period, and reacquire phase for the next report.
        if (observed_time > predicted_final && observed_time - predicted_final > gap_limit)
        {
            next_report_time_ = observed_time + recovered_time_t::convert(batch_period);

            return {
                .final_report_time = observed_time,
                .report_period = batch_period,
                .follows_gap = true,
            };
        }

        // Positive errors are already bounded by the gap test. Clamp large negative errors to the same magnitude: a
        // latency drop after a late reacquisition must not move the future report timeline backward or kick the period
        // estimator hard.
        auto const timing_error = bounded_error(observed_time, predicted_final, gap_limit);

        static constexpr auto rne_shifter = shifter_t<rounding_modes::shr::nearest_even>{};
        auto const phase_correction = multiply<timing_error_t, rne_shifter>(params_.phase_error_gain, timing_error);
        auto const period_correction = multiply<timing_error_t, rne_shifter>(params_.period_error_gain, timing_error);

        // Adriaensen update order: advance next time using the old period, then update the period estimate. The current
        // batch remains on the mapping predicted before observing it.
        next_report_time_ = add_signed(predicted_final + recovered_time_t::convert(batch_period), phase_correction);
        estimated_period_ = corrected_period(batch_period, period_correction);

        return {
            .final_report_time = predicted_final,
            .report_period = batch_period,
            .follows_gap = false,
        };
    }

    constexpr auto estimated_period() const noexcept -> period_t { return estimated_period_; }
    constexpr auto next_report_time() const noexcept -> recovered_time_t { return next_report_time_; }
    constexpr auto initialized() const noexcept -> bool { return initialized_; }

private:
    using report_count_fixed_t = fixed_t<report_count_t, 0>;

    static constexpr auto scale_period(period_t period, report_count_t report_count) noexcept -> recovered_time_t
    {
        auto const product = multiply(period, report_count_fixed_t::literal(report_count));
        return recovered_time_t::convert(product);
    }

    static constexpr auto bounded_error(
        recovered_time_t observed, recovered_time_t predicted, recovered_time_t limit) noexcept -> timing_error_t
    {
        if (observed >= predicted)
        {
            auto const magnitude = observed.value - predicted.value;
            assert(magnitude <= limit.value);
            assert(magnitude <= static_cast<uint128_t>(max<int64_t>()));
            return timing_error_t::literal(static_cast<int64_t>(magnitude));
        }

        auto magnitude = predicted.value - observed.value;
        if (magnitude > limit.value) magnitude = limit.value;

        assert(magnitude <= static_cast<uint128_t>(max<int64_t>()));
        auto const signed_magnitude = static_cast<int64_t>(magnitude);
        return timing_error_t::literal(-signed_magnitude);
    }

    static constexpr auto add_signed(recovered_time_t time, timing_error_t delta) noexcept -> recovered_time_t
    {
        if (delta.value >= 0) return recovered_time_t::literal(time.value + static_cast<uint64_t>(delta.value));

        auto const magnitude = recovered_time_t::convert(uabs(delta));
        assert(time >= magnitude);
        return time - magnitude;
    }

    constexpr auto corrected_period(period_t period, timing_error_t correction) const noexcept -> period_t
    {
        auto const magnitude = uabs(correction);

        if (correction >= timing_error_t{})
        {
            if (magnitude.value >= params_.maximum_period.value - period.value) return params_.maximum_period;
            return period_t::literal(period.value + magnitude.value);
        }
        else
        {
            if (magnitude.value >= period.value - params_.minimum_period.value) return params_.minimum_period;
            return period_t::literal(period.value - magnitude.value);
        }
    }

    params_t params_;
    recovered_time_t next_report_time_{};
    period_t estimated_period_{};
    timestamp_t previous_observed_time_{};
    bool initialized_{};
};

} // namespace crv::pipeline::filters
