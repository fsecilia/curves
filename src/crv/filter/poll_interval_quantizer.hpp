// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/math/shifter.hpp>
#include <cassert>

namespace crv {

/// quantizes jittered intervals between delivered input reports using a supplied polling period
///
/// Linux input may elide zero-displacement reports, and host-side timestamp latency can bunch adjacent delivered
/// reports. Consequently, one delivered report always consumes at least one polling tick, but consecutive delivered
/// reports may be separated by more than one tick.
///
/// The quantizer carries timing error forward:
///
///     corrected_interval = observed_interval + residual
///     elapsed_ticks      = max(1, round_to_nearest_even(corrected_interval / report_period))
///     residual           = corrected_interval - elapsed_ticks * report_period
///
/// Carrying residual makes timestamp compression and expansion repay one another instead of independently rounding
/// every observed interval.
///
/// The caller supplies one polling-period snapshot for each observation. That snapshot is used consistently for tick
/// assignment, logical-time advancement, residual calculation, and the returned interval. It is not retained.
///
/// This type does not recover absolute clock phase and does not estimate polling frequency.
class poll_interval_quantizer_t
{
public:
    /// Host timestamp in integral nanoseconds.
    using timestamp_t = fixed_t<uint64_t, 0>;

    /// Nanoseconds per hardware polling tick.
    ///
    /// Q32.32 covers periods up to almost 2^32 ns while retaining substantially more fractional precision than mouse
    /// polling requires.
    using period_t = fixed_t<uint64_t, 32>;

    /// Monotonic logical report time in nanoseconds.
    using quantized_time_t = fixed_t<uint128_t, period_t::frac_bits>;

    /// Signed carried timing error in nanoseconds.
    ///
    /// Within a continuity epoch, this is the difference between observed elapsed time and quantized elapsed time.
    /// A new epoch begins on initialization or timestamp regression.
    ///
    /// This is intentionally wide and additive-only. It is never multiplied.
    using residual_t = fixed_t<int128_t, period_t::frac_bits>;

    using tick_count_t = uint64_t;

    enum class status_t
    {
        /// First timestamp after construction or reset; no observed interval existed.
        initialized,

        /// Timestamp followed the previous timestamp monotonically and was quantized.
        continuous,

        /// Timestamp moved backward. Observation history was reanchored at the new timestamp.
        timestamp_regressed,
    };

    struct interval_t
    {
        /// Polling period used for this observation.
        period_t report_period;

        /// Polling ticks elapsed since the previous delivered report.
        ///
        /// Always at least one. Hidden zero-displacement ticks are `elapsed_ticks - 1`.
        tick_count_t elapsed_ticks;

        /// Monotonic logical report time accumulated from quantized intervals.
        quantized_time_t quantized_time;

        status_t status;

        /// True when the corrected interval was nonpositive or unconstrained nearest-even quantization selected zero,
        /// and the one-tick-per-delivered-report invariant supplied the result instead.
        ///
        /// This is normal timing information, not a continuity failure.
        bool minimum_tick_forced;

        constexpr auto hidden_zero_ticks() const noexcept -> tick_count_t
        {
            assert(elapsed_ticks != 0);
            return elapsed_ticks - 1;
        }

        /// Quantized report time rounded to integral nanoseconds for the kernel API.
        constexpr auto quantized_timestamp() const noexcept -> timestamp_t
        {
            static constexpr auto rne_shifter = shifter_t<rounding_modes::shr::nearest_even>{};
            return timestamp_t::template convert<rne_shifter>(quantized_time);
        }
    };

    constexpr poll_interval_quantizer_t() noexcept = default;

    /// Observes one delivered report timestamp using one polling-period snapshot.
    ///
    /// The first observation initializes timestamp state and returns one elapsed tick for processing the current
    /// delivered report. Its logical time remains the observed timestamp.
    ///
    /// A regressed timestamp is not silently quantized. Observation history is reanchored, carried error is cleared,
    /// and the existing logical timeline advances by one supplied report period.
    constexpr auto observe(timestamp_t timestamp, period_t report_period) noexcept -> interval_t
    {
        // Within a continuity epoch, a positive corrected interval cannot exceed the raw elapsed time since the epoch
        // anchor. Requiring at least one integral nanosecond per tick therefore keeps every tick count representable by
        // tick_count_t.
        assert(report_period >= period_t{1});

        if (!initialized_) [[unlikely]]
        {
            previous_timestamp_ = timestamp;
            residual_ = {};
            quantized_time_ = quantized_time_t::convert(timestamp);
            initialized_ = true;

            return {
                .report_period = report_period,
                .elapsed_ticks = 1,
                .quantized_time = quantized_time_,
                .status = status_t::initialized,
                .minimum_tick_forced = false,
            };
        }

        if (timestamp < previous_timestamp_) [[unlikely]]
        {
            // Preserve the downstream logical timeline while beginning a new raw-timestamp continuity epoch.
            previous_timestamp_ = timestamp;
            residual_ = {};

            quantized_time_ += quantized_time_t::convert(report_period);

            return {
                .report_period = report_period,
                .elapsed_ticks = 1,
                .quantized_time = quantized_time_,
                .status = status_t::timestamp_regressed,
                .minimum_tick_forced = false,
            };
        }

        auto const observed_interval = timestamp - previous_timestamp_;
        previous_timestamp_ = timestamp;

        auto const corrected_interval = residual_t::convert(observed_interval) + residual_;

        auto const quantized = quantize(corrected_interval, report_period);

        // u64 Q32 * u64 Q0 -> u128 Q32. No 128-bit operand is multiplied.
        auto const quantized_interval = multiply(report_period, tick_count_fixed_t::literal(quantized.elapsed_ticks));

        // u128 Q32 accumulator plus u128 Q32 interval.
        quantized_time_ += quantized_time_t::convert(quantized_interval);

        residual_ = corrected_interval - residual_t::convert(quantized_interval);

        return {
            .report_period = report_period,
            .elapsed_ticks = quantized.elapsed_ticks,
            .quantized_time = quantized_time_,
            .status = status_t::continuous,
            .minimum_tick_forced = quantized.minimum_tick_forced,
        };
    }

    constexpr void reset() noexcept
    {
        quantized_time_ = {};
        previous_timestamp_ = {};
        residual_ = {};
        initialized_ = false;
    }

    constexpr auto previous_timestamp() const noexcept -> timestamp_t
    {
        assert(initialized_);
        return previous_timestamp_;
    }

    constexpr auto residual() const noexcept -> residual_t { return residual_; }
    constexpr auto initialized() const noexcept -> bool { return initialized_; }

private:
    using positive_interval_t = fixed_t<uint128_t, period_t::frac_bits>;
    using tick_count_fixed_t = fixed_t<tick_count_t, 0>;

    struct quantized_tick_count_t
    {
        tick_count_t elapsed_ticks;
        bool minimum_tick_forced;
    };

    static constexpr auto quantize(residual_t corrected_interval, period_t report_period) noexcept
        -> quantized_tick_count_t
    {
        if (corrected_interval <= residual_t{})
        {
            return {
                .elapsed_ticks = 1,
                .minimum_tick_forced = true,
            };
        }

        auto const positive_interval = positive_interval_t::convert(corrected_interval);

        // u128 Q32 / u64 Q32 -> u64 Q0.
        auto const rounded_ticks
            = divide<tick_count_fixed_t>(positive_interval, report_period, rounding_modes::div::nearest_even);

        if (rounded_ticks.value == 0)
        {
            return {
                .elapsed_ticks = 1,
                .minimum_tick_forced = true,
            };
        }

        return {
            .elapsed_ticks = rounded_ticks.value,
            .minimum_tick_forced = false,
        };
    }

    quantized_time_t quantized_time_{};
    timestamp_t previous_timestamp_{};
    residual_t residual_{};
    bool initialized_{};
};

} // namespace crv
