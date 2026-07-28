// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include "poll_interval_quantizer.hpp"
#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/limits.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/math/shifter.hpp>
#include <cassert>

namespace crv {

/// tracks slow polling-period drift from raw timestamp spans
///
/// The interval quantizer determines how many polling ticks elapsed between delivered reports. This tracker estimates
/// the broad polling period from long raw timestamp spans:
///
///     measured_period = timestamp_span/accumulated_ticks
///     period_error    = measured_period - estimated_period
///     correction      = clamp(rne_shift(period_error, gain_shift), -maximum_correction, +maximum_correction)
///
/// Measurements whose error exceeds maximum_measurement_error are rejected. Large individual tick gaps discard the
/// current measurement window.
///
/// The tracker must be explicitly anchored before observe() is called. This keeps the quantizer's synthetic
/// initialization tick out of the first measurement window.
class poll_period_tracker_t
{
public:
    using timestamp_t = poll_interval_quantizer_t::timestamp_t;
    using period_t = poll_interval_quantizer_t::period_t;
    using tick_count_t = poll_interval_quantizer_t::tick_count_t;
    using period_error_t = fixed_t<int128_t, period_t::frac_bits>;

    struct params_t
    {
        period_t initial_period;

        tick_count_t measurement_window_ticks;
        tick_count_t maximum_training_gap_ticks;

        unsigned gain_shift;

        period_t maximum_measurement_error;
        period_t maximum_correction;

        period_t minimum_period;
        period_t maximum_period;
    };

    enum class status_t
    {
        accumulating,
        updated,
        rejected,
        reanchored,
    };

    struct update_t
    {
        period_t estimated_period;

        // meaningful only when has_measurement() is true
        period_t measured_period;

        // actual change applied to estimated_period after all limits; zero unless status is updated
        period_error_t correction;

        status_t status;

        constexpr auto has_measurement() const noexcept -> bool
        {
            return status == status_t::updated || status == status_t::rejected;
        }
    };

    constexpr explicit poll_period_tracker_t(params_t params) noexcept : params_{params}
    {
        validate_params();
        reset();
    }

    /// consumes one continuous raw timestamp interval
    ///
    /// elapsed_ticks describes the interval ending at timestamp. The tracker must already have been anchored at an
    /// earlier raw timestamp.
    [[nodiscard]] constexpr auto observe(timestamp_t timestamp, tick_count_t elapsed_ticks) noexcept -> update_t
    {
        assert(anchored_);
        assert(elapsed_ticks != 0);

        // timestamp regressions are translated to reanchor() by orchestrator rather than consumed as frequency evidence
        assert(timestamp >= window_start_timestamp_);

        if (elapsed_ticks > params_.maximum_training_gap_ticks)
        {
            reanchor(timestamp);
            return make_update(status_t::reanchored);
        }

        // by construction, this cannot overflow while a window remains below measurement_window_ticks
        window_ticks_ += elapsed_ticks;

        if (window_ticks_ < params_.measurement_window_ticks) return make_update(status_t::accumulating);

        auto const measured_period = measure_period(timestamp);

        // measurement_window_ticks is a minimum window length. If this observation crosses the threshold, measure using
        // all of its ticks and the current raw timestamp. Splitting it would require inventing a raw timestamp for a
        // hidden polling boundary.
        reanchor(timestamp);

        auto const period_error = to_error(measured_period) - to_error(estimated_period_);
        auto const maximum_measurement_error = to_error(params_.maximum_measurement_error);
        if (abs(period_error) > maximum_measurement_error) { return make_update(status_t::rejected, measured_period); }

        auto const correction = apply_correction(period_error);
        return make_update(status_t::updated, measured_period, correction);
    }

    /// establishes new raw timestamp anchor while preserving estimate
    constexpr void reanchor(timestamp_t timestamp) noexcept
    {
        window_start_timestamp_ = timestamp;
        window_ticks_ = 0;
        anchored_ = true;
    }

    /// restores configured initial estimate and removes raw anchor
    constexpr void reset() noexcept
    {
        estimated_period_ = params_.initial_period;

        window_start_timestamp_ = {};
        window_ticks_ = 0;
        anchored_ = false;
    }

    constexpr auto estimated_period() const noexcept -> period_t { return estimated_period_; }
    constexpr auto anchored() const noexcept -> bool { return anchored_; }

private:
    using tick_span_t = fixed_t<tick_count_t, 0>;

    static constexpr auto rne_shifter = shifter_t<rounding_modes::shr::nearest_even>{};

    constexpr void validate_params() const noexcept
    {
        assert(params_.minimum_period >= period_t{1});
        assert(params_.minimum_period <= params_.initial_period);
        assert(params_.initial_period <= params_.maximum_period);

        assert(params_.measurement_window_ticks != 0);
        assert(params_.maximum_training_gap_ticks != 0);

        assert(params_.gain_shift < static_cast<unsigned>(period_error_t::container_bits));

        // before an addition:
        //
        //     window_ticks_ < measurement_window_ticks
        //     elapsed_ticks <= maximum_training_gap_ticks
        //
        // window_ticks_ += elapsed_ticks is safe
        assert(params_.measurement_window_ticks - 1 <= crv::max<tick_count_t>() - params_.maximum_training_gap_ticks);
    }

    constexpr auto measure_period(timestamp_t timestamp) const noexcept -> period_t
    {
        assert(window_ticks_ >= params_.measurement_window_ticks);
        assert(window_ticks_ != 0);
        assert(timestamp >= window_start_timestamp_);

        auto const timestamp_span = timestamp - window_start_timestamp_;

        auto const tick_span = tick_span_t::literal(window_ticks_);

        // (u64 Q0)/(u64 Q0) -> (u64 Q32), nearest even
        return divide<period_t>(timestamp_span, tick_span, rounding_modes::div::nearest_even);
    }

    static constexpr auto to_error(period_t period) noexcept -> period_error_t
    {
        return period_error_t::convert(period);
    }

    static constexpr auto clamp_value(period_error_t value, period_error_t minimum, period_error_t maximum) noexcept
        -> period_error_t
    {
        assert(minimum <= maximum);

        if (value < minimum) return minimum;
        if (value > maximum) return maximum;
        return value;
    }

    constexpr auto apply_gain(period_error_t period_error) const noexcept -> period_error_t
    {
        if (params_.gain_shift == 0) { return period_error; }
        return period_error_t::literal(rne_shifter.shr(period_error.value, params_.gain_shift));
    }

    constexpr auto apply_correction(period_error_t period_error) noexcept -> period_error_t
    {
        auto const maximum_correction = to_error(params_.maximum_correction);

        auto const requested_correction
            = clamp_value(apply_gain(period_error), -maximum_correction, maximum_correction);

        auto const previous_estimate = to_error(estimated_period_);
        auto const minimum_period = to_error(params_.minimum_period);
        auto const maximum_period = to_error(params_.maximum_period);

        auto const updated_estimate
            = clamp_value(previous_estimate + requested_correction, minimum_period, maximum_period);

        // conversion is exact: both types are Q32, and updated_estimate is clamped
        estimated_period_ = period_t::convert(updated_estimate);

        // report what actually moved after the period bounds, not just the larger requested correction
        return updated_estimate - previous_estimate;
    }

    constexpr auto make_update(
        status_t status, period_t measured_period = {}, period_error_t correction = {}) const noexcept -> update_t
    {
        return {
            .estimated_period = estimated_period_,
            .measured_period = measured_period,
            .correction = correction,
            .status = status,
        };
    }

    params_t params_;

    period_t estimated_period_{};

    timestamp_t window_start_timestamp_{};
    tick_count_t window_ticks_{};

    bool anchored_{};
};

} // namespace crv
