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

/// quantizes delivered report timestamps onto a polling timeline
///
/// Zero-displacement reports may be absent, so each delivered report consumes at least one tick while one observation
/// may represent multiple elapsed ticks.
///
///     corrected_interval = observed_interval + residual
///     elapsed_ticks      = max(1, RNE(corrected_interval/report_period))
///     residual           = corrected_interval - elapsed_ticks * report_period
class poll_interval_quantizer_t
{
public:
    using timestamp_t = fixed_t<uint64_t, 0>;

    // Q32.32 covers sub-nanosecond periods without exceeding the range needed for input-device polling
    using period_t = fixed_t<uint64_t, 32>;

    using quantized_time_t = fixed_t<uint128_t, period_t::frac_bits>;

    // wide because minimum-tick forcing permits residual below -report_period/2
    using residual_t = fixed_t<int128_t, period_t::frac_bits>;

    using tick_count_t = uint64_t;

    enum class status_t
    {
        initialized,
        continuous,

        // raw time reanchored after moving backward; logical time remains monotonic
        timestamp_regressed,
    };

    struct interval_t
    {
        period_t report_period;

        // hidden zero-displacement ticks are elapsed_ticks - 1
        tick_count_t elapsed_ticks;

        quantized_time_t quantized_time;
        status_t status;

        // true when nonpositive or zero-tick quantization was clamped to one tick
        bool minimum_tick_forced;

        constexpr auto hidden_zero_ticks() const noexcept -> tick_count_t
        {
            assert(elapsed_ticks != 0);
            return elapsed_ticks - 1;
        }

        constexpr auto quantized_timestamp() const noexcept -> timestamp_t
        {
            static constexpr auto rne_shifter = shifter_t<rounding_modes::shr::nearest_even>{};

            return timestamp_t::template convert<rne_shifter>(quantized_time);
        }
    };

    constexpr poll_interval_quantizer_t() noexcept = default;

    constexpr auto observe(timestamp_t timestamp, period_t report_period) noexcept -> interval_t
    {
        // keep every possible timestamp interval representable as tick_count_t
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

        // (u64 Q32)*(u64 Q0) -> (u128 Q32)
        auto const quantized_interval = multiply(report_period, tick_count_fixed_t::literal(quantized.elapsed_ticks));

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

        // (u128 Q32)/(u64 Q32) -> (u64 Q0)
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
