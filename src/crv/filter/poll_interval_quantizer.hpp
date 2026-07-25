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

/// quantizes jittered intervals between delivered input reports onto a fixed polling-period grid
///
/// Linux input may elide zero-displacement reports, and host-side timestamp latency can bunch adjacent delivered
/// reports. Consequently, one delivered report always consumes at least one polling tick, but consecutive delivered
/// reports may be separated by more than one tick.
///
/// The quantizer carries timing error forward:
///
///     corrected_interval = observed_interval + residual
///     elapsed_ticks      = max(1, round_to_nearest_even(corrected_interval / period))
///     residual           = corrected_interval - elapsed_ticks * period
///
/// Carrying residual makes timestamp compression and expansion repay one another instead of independently rounding
/// every observed interval.
///
/// This type does not recover absolute clock phase and does not estimate polling frequency. The configured polling
/// period is fixed for the lifetime of the object.
class poll_interval_quantizer_t
{
public:
    /// Host timestamp in integral nanoseconds.
    using timestamp_t = fixed_t<uint64_t, 0>;

    /// Configured nanoseconds per hardware polling tick.
    ///
    /// Q32.32 covers periods up to almost 2^32 ns while retaining substantially more fractional precision than mouse
    /// polling requires.
    using period_t = fixed_t<uint64_t, 32>;
    using quantized_time_t = fixed_t<uint128_t, period_t::frac_bits>;

    /// Signed carried timing error in nanoseconds.
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

        /// Timestamp moved backward. State was reanchored at the new timestamp.
        timestamp_regressed,
    };

    struct interval_t
    {
        /// Polling period to use for the current delivered report.
        period_t report_period;

        /// Polling ticks elapsed since the previous delivered report.
        ///
        /// Always at least one. Hidden zero-displacement ticks are `elapsed_ticks - 1`.
        tick_count_t elapsed_ticks;

        /// Absolute logical report time on the quantized polling grid.
        quantized_time_t quantized_time;

        status_t status;

        /// True when unconstrained nearest-even quantization selected zero or a negative tick count and the
        /// one-tick-per-delivered-report invariant supplied the result instead.
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

    constexpr explicit poll_interval_quantizer_t(period_t polling_period) noexcept : polling_period_{polling_period}
    {
        // Integral host timestamps span at most UINT64_MAX ns. Requiring at least one ns per tick makes every valid
        // interval's tick count fit tick_count_t. Mouse periods are many orders of magnitude larger.
        assert(polling_period_ >= period_t::literal(1));
    }

    /// Observes one delivered report timestamp.
    ///
    /// The first observation initializes timestamp state and returns one report period for processing the current
    /// displacement.
    ///
    /// A regressed timestamp is not silently quantized. The quantizer reanchors, clears carried error, and reports
    /// timestamp_regressed.
    constexpr auto observe(timestamp_t timestamp) noexcept -> interval_t
    {
        if (!initialized_) [[unlikely]]
        {
            previous_timestamp_ = timestamp;
            residual_ = {};
            quantized_time_ = quantized_time_t::convert(timestamp);
            initialized_ = true;

            return {
                .report_period = polling_period_,
                .elapsed_ticks = 1,
                .quantized_time = quantized_time_,
                .status = status_t::initialized,
                .minimum_tick_forced = false,
            };
        }

        if (timestamp < previous_timestamp_) [[unlikely]]
        {
            // The observed clock is discontinuous. Reanchor observation history at the new timestamp, but preserve the
            // downstream logical timeline.
            //
            // This delivered report must occur after the previous delivered report, so advance the logical clock by the
            // minimum valid interval: one tick.
            previous_timestamp_ = timestamp;
            residual_ = {};

            quantized_time_ += quantized_time_t::convert(polling_period_);

            return {
                .report_period = polling_period_,
                .elapsed_ticks = 1,
                .quantized_time = quantized_time_,
                .status = status_t::timestamp_regressed,
                .minimum_tick_forced = false,
            };
        }
        auto const observed_interval = timestamp - previous_timestamp_;
        previous_timestamp_ = timestamp;

        auto const corrected_interval = residual_t::convert(observed_interval) + residual_;

        auto const quantized = quantize(corrected_interval);

        using tick_count_fixed_t = fixed_t<tick_count_t, 0>;

        // u64 Q32 * u64 Q0 -> u128 Q32. No 128-bit operand is multiplied.
        auto const quantized_interval = multiply(polling_period_, tick_count_fixed_t::literal(quantized.elapsed_ticks));

        // u128 Q32 accumulator plus u128 Q32 interval.
        quantized_time_ += quantized_time_t::convert(quantized_interval);

        residual_ = corrected_interval - residual_t::convert(quantized_interval);

        return {
            .report_period = polling_period_,
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

    constexpr auto polling_period() const noexcept -> period_t { return polling_period_; }

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

    /// Rounds a positive interval/period ratio to nearest-even integral ticks.
    ///
    /// fixed-point divide truncates to the requested Q64.0 output. The remainder is then compared with its complement
    /// inside one period:
    ///
    ///     remainder > period - remainder  <=>  remainder > period / 2
    ///
    /// This avoids doubling a u128 remainder, which would require a 256-bit multiplication result.
    constexpr auto round_positive_interval(positive_interval_t interval) const noexcept -> tick_count_t
    {
        auto const floor_ticks = divide<tick_count_fixed_t>(interval, polling_period_);

        auto const floor_interval = positive_interval_t::convert(multiply(polling_period_, floor_ticks));

        auto const remainder = interval - floor_interval;
        auto const period = positive_interval_t::convert(polling_period_);
        auto const remainder_complement = period - remainder;

        auto const above_half = remainder > remainder_complement;
        auto const exactly_half = remainder == remainder_complement;
        auto const floor_is_odd = (floor_ticks.value & tick_count_t{1}) != 0;

        if (above_half || (exactly_half && floor_is_odd))
        {
            assert(floor_ticks.value != max<tick_count_t>());
            return floor_ticks.value + 1;
        }

        return floor_ticks.value;
    }

    constexpr auto quantize(residual_t corrected_interval) const noexcept -> quantized_tick_count_t
    {
        if (corrected_interval <= residual_t{})
        {
            return {
                .elapsed_ticks = 1,
                .minimum_tick_forced = true,
            };
        }

        auto const positive_interval = positive_interval_t::convert(corrected_interval);

        auto const rounded_ticks = round_positive_interval(positive_interval);

        if (rounded_ticks == 0)
        {
            return {
                .elapsed_ticks = 1,
                .minimum_tick_forced = true,
            };
        }

        return {
            .elapsed_ticks = rounded_ticks,
            .minimum_tick_forced = false,
        };
    }

    period_t polling_period_;
    quantized_time_t quantized_time_{};
    timestamp_t previous_timestamp_{};
    residual_t residual_{};
    bool initialized_{};
};

} // namespace crv
