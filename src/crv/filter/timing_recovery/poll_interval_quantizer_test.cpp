// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "poll_interval_quantizer.hpp"
#include <crv/test/test.hpp>
#include <array>
#include <limits>

namespace crv {
namespace {

using sut_t = poll_interval_quantizer_t;
using timestamp_t = sut_t::timestamp_t;
using period_t = sut_t::period_t;
using residual_t = sut_t::residual_t;
using status_t = sut_t::status_t;
using tick_count_fixed_t = fixed_t<sut_t::tick_count_t, 0>;

constexpr auto timestamp(uint64_t nanoseconds) noexcept -> timestamp_t
{
    return timestamp_t{nanoseconds};
}

constexpr auto period(uint64_t nanoseconds) noexcept -> period_t
{
    return period_t{nanoseconds};
}

constexpr auto period_plus_half_nanosecond(uint64_t nanoseconds) noexcept -> period_t
{
    return period_t{nanoseconds} + period_t::literal(uint64_t{1} << (period_t::frac_bits - 1));
}

constexpr auto residual(int64_t nanoseconds) noexcept -> residual_t
{
    return residual_t{nanoseconds};
}

constexpr auto negative_half_nanosecond() noexcept -> residual_t
{
    return residual_t::literal(-(int128_t{1} << (period_t::frac_bits - 1)));
}

constexpr auto initializes_without_inventing_hidden_ticks() noexcept -> bool
{
    auto sut = sut_t{};
    auto const report_period = period(100);

    auto const result = sut.observe(timestamp(1'000), report_period);

    return result.report_period == report_period && result.elapsed_ticks == 1 && result.hidden_zero_ticks() == 0
        && result.status == status_t::initialized && !result.minimum_tick_forced
        && sut.previous_timestamp() == timestamp(1'000) && sut.residual() == residual_t{} && sut.initialized();
}
static_assert(initializes_without_inventing_hidden_ticks());

constexpr auto exact_dense_stream_returns_one_tick() noexcept -> bool
{
    auto sut = sut_t{};
    auto const report_period = period(100);

    static_cast<void>(sut.observe(timestamp(1'000), report_period));
    auto const first = sut.observe(timestamp(1'100), report_period);
    auto const second = sut.observe(timestamp(1'200), report_period);

    return first.report_period == report_period && second.report_period == report_period && first.elapsed_ticks == 1
        && second.elapsed_ticks == 1 && first.status == status_t::continuous && second.status == status_t::continuous
        && !first.minimum_tick_forced && !second.minimum_tick_forced && sut.residual() == residual_t{};
}
static_assert(exact_dense_stream_returns_one_tick());

constexpr auto exact_sparse_stream_reports_all_elapsed_ticks() noexcept -> bool
{
    auto sut = sut_t{};
    auto const report_period = period(100);

    static_cast<void>(sut.observe(timestamp(1'000), report_period));
    auto const result = sut.observe(timestamp(1'700), report_period);

    return result.report_period == report_period && result.elapsed_ticks == 7 && result.hidden_zero_ticks() == 6
        && !result.minimum_tick_forced && sut.residual() == residual_t{};
}
static_assert(exact_sparse_stream_reports_all_elapsed_ticks());

constexpr auto short_then_long_intervals_repay_timing_debt() noexcept -> bool
{
    auto sut = sut_t{};
    auto const report_period = period(100);

    static_cast<void>(sut.observe(timestamp(1'000), report_period));

    // residual = 20 - 100 = -80 ns
    auto const short_result = sut.observe(timestamp(1'020), report_period);

    if (short_result.elapsed_ticks != 1) return false;
    if (!short_result.minimum_tick_forced) return false;
    if (sut.residual() != residual(-80)) return false;

    // corrected = 180 - 80 = 100 ns
    auto const long_result = sut.observe(timestamp(1'200), report_period);

    return long_result.elapsed_ticks == 1 && !long_result.minimum_tick_forced && sut.residual() == residual_t{};
}
static_assert(short_then_long_intervals_repay_timing_debt());

constexpr auto equal_timestamps_consume_one_tick_and_carry_debt() noexcept -> bool
{
    auto sut = sut_t{};
    auto const report_period = period(100);

    static_cast<void>(sut.observe(timestamp(1'000), report_period));

    auto const duplicate = sut.observe(timestamp(1'000), report_period);

    if (duplicate.elapsed_ticks != 1) return false;
    if (!duplicate.minimum_tick_forced) return false;
    if (sut.residual() != residual(-100)) return false;

    auto const repayment = sut.observe(timestamp(1'200), report_period);

    return repayment.elapsed_ticks == 1 && !repayment.minimum_tick_forced && sut.residual() == residual_t{};
}
static_assert(equal_timestamps_consume_one_tick_and_carry_debt());

constexpr auto repeated_timestamps_preserve_debt_until_repaid() noexcept -> bool
{
    constexpr auto duplicate_count = uint64_t{8};

    auto sut = sut_t{};
    auto const report_period = period(100);

    static_cast<void>(sut.observe(timestamp(1'000), report_period));

    for (auto count = uint64_t{1}; count <= duplicate_count; ++count)
    {
        auto const result = sut.observe(timestamp(1'000), report_period);

        if (result.elapsed_ticks != 1) return false;
        if (!result.minimum_tick_forced) return false;

        if (sut.residual() != residual(-static_cast<int64_t>(count * 100))) { return false; }
    }

    auto const repayment = sut.observe(timestamp(1'000 + (duplicate_count + 1) * 100), report_period);

    return repayment.elapsed_ticks == 1 && !repayment.minimum_tick_forced && sut.residual() == residual_t{};
}
static_assert(repeated_timestamps_preserve_debt_until_repaid());

constexpr auto half_tick_ties_round_to_even() noexcept -> bool
{
    {
        auto sut = sut_t{};
        auto const report_period = period(100);

        static_cast<void>(sut.observe(timestamp(1'000), report_period));

        // 2.5 ticks rounds to even tick count 2.
        auto const result = sut.observe(timestamp(1'250), report_period);

        if (result.elapsed_ticks != 2) return false;
        if (sut.residual() != residual(50)) return false;
    }

    {
        auto sut = sut_t{};
        auto const report_period = period(100);

        static_cast<void>(sut.observe(timestamp(1'000), report_period));

        // 3.5 ticks rounds to even tick count 4.
        auto const result = sut.observe(timestamp(1'350), report_period);

        if (result.elapsed_ticks != 4) return false;
        if (sut.residual() != residual(-50)) return false;
    }

    return true;
}
static_assert(half_tick_ties_round_to_even());

constexpr auto half_tick_zero_tie_is_forced_to_one() noexcept -> bool
{
    auto sut = sut_t{};
    auto const report_period = period(100);

    static_cast<void>(sut.observe(timestamp(1'000), report_period));

    // nearest-even selects zero; the delivered report still consumes one tick
    auto const result = sut.observe(timestamp(1'050), report_period);

    return result.elapsed_ticks == 1 && result.minimum_tick_forced && sut.residual() == residual(-50);
}
static_assert(half_tick_zero_tie_is_forced_to_one());

constexpr auto fractional_period_trace_remains_bounded() noexcept -> bool
{
    constexpr auto pair_count = uint64_t{256};

    auto sut = sut_t{};
    auto const report_period = period_plus_half_nanosecond(100);

    auto current_timestamp = uint64_t{1'000};

    static_cast<void>(sut.observe(timestamp(current_timestamp), report_period));

    for (auto pair = uint64_t{}; pair < pair_count; ++pair)
    {
        current_timestamp += 100;

        auto const shorter = sut.observe(timestamp(current_timestamp), report_period);

        if (shorter.elapsed_ticks != 1) return false;
        if (shorter.minimum_tick_forced) return false;
        if (sut.residual() != negative_half_nanosecond()) return false;

        current_timestamp += 101;

        auto const longer = sut.observe(timestamp(current_timestamp), report_period);

        if (longer.elapsed_ticks != 1) return false;
        if (longer.minimum_tick_forced) return false;
        if (sut.residual() != residual_t{}) return false;
    }

    return true;
}
static_assert(fractional_period_trace_remains_bounded());

constexpr auto varying_sparse_intervals_preserve_tick_counts() noexcept -> bool
{
    constexpr auto expected_ticks = std::array<uint64_t, 5>{4, 7, 2, 11, 3};

    auto sut = sut_t{};
    auto const report_period = period(100);
    auto current_timestamp = uint64_t{1'000};

    static_cast<void>(sut.observe(timestamp(current_timestamp), report_period));

    for (auto const ticks : expected_ticks)
    {
        current_timestamp += ticks * 100;

        auto const result = sut.observe(timestamp(current_timestamp), report_period);

        if (result.report_period != report_period) return false;
        if (result.elapsed_ticks != ticks) return false;
        if (result.minimum_tick_forced) return false;
        if (sut.residual() != residual_t{}) return false;
    }

    return true;
}
static_assert(varying_sparse_intervals_preserve_tick_counts());

constexpr auto varying_periods_apply_to_current_intervals_and_conserve_time() noexcept -> bool
{
    auto sut = sut_t{};

    static_cast<void>(sut.observe(timestamp(1'000), period(100)));

    // corrected = 110 ns; residual = +10 ns
    auto const first = sut.observe(timestamp(1'110), period(100));

    if (first.report_period != period(100)) return false;
    if (first.elapsed_ticks != 1) return false;
    if (sut.residual() != residual(10)) return false;

    // corrected = 120 + 10 = 130 ns; residual = +10 ns
    auto const second = sut.observe(timestamp(1'230), period(120));

    if (second.report_period != period(120)) return false;
    if (second.elapsed_ticks != 1) return false;
    if (sut.residual() != residual(10)) return false;

    // corrected = 110 + 10 = 120 ns = 1.5 ticks; RNE selects 2.
    auto const third = sut.observe(timestamp(1'340), period(80));

    if (third.report_period != period(80)) return false;
    if (third.elapsed_ticks != 2) return false;
    if (sut.residual() != residual(-40)) return false;

    return residual(340) == residual(380) + sut.residual();
}
static_assert(varying_periods_apply_to_current_intervals_and_conserve_time());

constexpr auto minimum_tick_forcing_uses_current_period() noexcept -> bool
{
    auto sut = sut_t{};

    static_cast<void>(sut.observe(timestamp(1'000), period(100)));

    // residual = 20 - 120 = -100 ns
    auto const forced = sut.observe(timestamp(1'020), period(120));

    if (forced.report_period != period(120)) return false;
    if (forced.elapsed_ticks != 1) return false;
    if (!forced.minimum_tick_forced) return false;
    if (sut.residual() != residual(-100)) return false;

    // corrected = 180 - 100 = 80 ns
    auto const repaid = sut.observe(timestamp(1'200), period(80));

    return repaid.report_period == period(80) && repaid.elapsed_ticks == 1 && !repaid.minimum_tick_forced
        && sut.residual() == residual_t{};
}
static_assert(minimum_tick_forcing_uses_current_period());

constexpr auto timestamp_regression_uses_current_period_and_reanchors() noexcept -> bool
{
    auto sut = sut_t{};

    static_cast<void>(sut.observe(timestamp(1'000), period(100)));

    static_cast<void>(sut.observe(timestamp(1'020), period(100)));

    if (sut.residual() != residual(-80)) return false;

    auto const regression = sut.observe(timestamp(900), period(125));

    if (regression.report_period != period(125)) return false;
    if (regression.status != status_t::timestamp_regressed) return false;
    if (regression.elapsed_ticks != 1) return false;
    if (regression.minimum_tick_forced) return false;
    if (sut.previous_timestamp() != timestamp(900)) return false;
    if (sut.residual() != residual_t{}) return false;

    auto const recovered = sut.observe(timestamp(1'025), period(125));

    return recovered.report_period == period(125) && recovered.status == status_t::continuous
        && recovered.elapsed_ticks == 1 && !recovered.minimum_tick_forced && sut.residual() == residual_t{};
}
static_assert(timestamp_regression_uses_current_period_and_reanchors());

constexpr auto reset_restores_uninitialized_state_without_retaining_a_period() noexcept -> bool
{
    auto sut = sut_t{};

    static_cast<void>(sut.observe(timestamp(1'000), period(100)));
    static_cast<void>(sut.observe(timestamp(1'020), period(100)));

    sut.reset();

    if (sut.initialized()) return false;
    if (sut.residual() != residual_t{}) return false;

    auto const result = sut.observe(timestamp(9'000), period(125));

    return result.report_period == period(125) && result.status == status_t::initialized && result.elapsed_ticks == 1
        && !result.minimum_tick_forced && sut.residual() == residual_t{};
}
static_assert(reset_restores_uninitialized_state_without_retaining_a_period());

constexpr auto minimum_period_supports_maximum_timestamp_range() noexcept -> bool
{
    constexpr auto maximum_timestamp = std::numeric_limits<uint64_t>::max();

    auto sut = sut_t{};
    auto const report_period = period(1);

    static_cast<void>(sut.observe(timestamp(0), report_period));

    auto const result = sut.observe(timestamp(maximum_timestamp), report_period);

    return result.elapsed_ticks == maximum_timestamp && !result.minimum_tick_forced && sut.residual() == residual_t{};
}
static_assert(minimum_period_supports_maximum_timestamp_range());

constexpr auto mixed_fixed_period_trace_conserves_elapsed_time() noexcept -> bool
{
    constexpr auto timestamps = std::array<uint64_t, 7>{
        1'000,
        1'020,
        1'200,
        1'500,
        1'750,
        1'760,
        2'200,
    };

    auto sut = sut_t{};
    auto const report_period = period(100);

    auto observed_sum = residual_t{};
    auto quantized_sum = residual_t{};

    static_cast<void>(sut.observe(timestamp(timestamps.front()), report_period));

    for (auto index = std::size_t{1}; index < timestamps.size(); ++index)
    {
        auto const raw_delta = timestamp_t{timestamps[index]} - timestamp_t{timestamps[index - 1]};

        auto const result = sut.observe(timestamp(timestamps[index]), report_period);

        auto const quantized_interval
            = multiply(result.report_period, tick_count_fixed_t::literal(result.elapsed_ticks));

        observed_sum += residual_t::convert(raw_delta);
        quantized_sum += residual_t::convert(quantized_interval);
    }

    return observed_sum == quantized_sum + sut.residual() && observed_sum == residual(1'200)
        && quantized_sum == residual(1'200) && sut.residual() == residual_t{};
}
static_assert(mixed_fixed_period_trace_conserves_elapsed_time());

TEST(poll_interval_quantizer_test, long_bunched_trace_conserves_elapsed_time)
{
    constexpr auto polling_period_nanoseconds = uint64_t{250'000};

    auto sut = sut_t{};
    auto const report_period = period(polling_period_nanoseconds);

    constexpr auto interval_pattern = std::array<uint64_t, 4>{
        10'000,
        15'000,
        25'000,
        950'000,
    };

    auto current_timestamp = uint64_t{1'000'000};
    auto observed_sum = residual_t{};
    auto quantized_sum = residual_t{};
    auto forced_count = uint64_t{};

    static_cast<void>(sut.observe(timestamp(current_timestamp), report_period));

    for (auto repetition = 0; repetition < 10'000; ++repetition)
    {
        for (auto const raw_interval : interval_pattern)
        {
            current_timestamp += raw_interval;

            auto const result = sut.observe(timestamp(current_timestamp), report_period);

            observed_sum += residual_t::convert(timestamp_t{raw_interval});

            quantized_sum += residual_t::convert(
                multiply(result.report_period, tick_count_fixed_t::literal(result.elapsed_ticks)));

            if (result.minimum_tick_forced) ++forced_count;
        }
    }

    EXPECT_GT(forced_count, 0U);
    EXPECT_EQ(observed_sum, quantized_sum + sut.residual());
    EXPECT_EQ(sut.residual(), residual_t{});
}

// --- dev-only absolute timestamp projection ------------------------------------

constexpr auto absolute_projection_initializes_at_raw_time() noexcept -> bool
{
    auto sut = sut_t{};

    auto const result = sut.observe(timestamp(1'000), period(100));

    return result.quantized_timestamp() == timestamp(1'000);
}
static_assert(absolute_projection_initializes_at_raw_time());

constexpr auto absolute_projection_accumulates_varying_periods() noexcept -> bool
{
    auto sut = sut_t{};

    auto const initialized = sut.observe(timestamp(1'000), period(100));
    if (initialized.quantized_timestamp() != timestamp(1'000)) return false;

    auto const first = sut.observe(timestamp(1'110), period(100));
    if (first.quantized_timestamp() != timestamp(1'100)) return false;

    auto const second = sut.observe(timestamp(1'230), period(120));
    if (second.quantized_timestamp() != timestamp(1'220)) return false;

    auto const third = sut.observe(timestamp(1'340), period(80));
    return third.quantized_timestamp() == timestamp(1'380);
}
static_assert(absolute_projection_accumulates_varying_periods());

constexpr auto absolute_projection_remains_monotonic_after_regression() noexcept -> bool
{
    auto sut = sut_t{};

    static_cast<void>(sut.observe(timestamp(1'000), period(100)));

    auto const continuous = sut.observe(timestamp(1'100), period(100));
    if (continuous.quantized_timestamp() != timestamp(1'100)) return false;

    auto const regression = sut.observe(timestamp(900), period(125));
    if (regression.quantized_timestamp() != timestamp(1'225)) return false;

    auto const recovered = sut.observe(timestamp(1'025), period(125));
    return recovered.quantized_timestamp() == timestamp(1'350);
}
static_assert(absolute_projection_remains_monotonic_after_regression());

constexpr auto absolute_projection_rounds_fractional_time_for_kernel_api() noexcept -> bool
{
    auto sut = sut_t{};
    auto const report_period = period_plus_half_nanosecond(100);

    auto const initialized = sut.observe(timestamp(1'000), report_period);
    if (initialized.quantized_timestamp() != timestamp(1'000)) return false;

    // 1'100.5 ns rounds to even integer 1'100.
    auto const first = sut.observe(timestamp(1'100), report_period);
    if (first.quantized_timestamp() != timestamp(1'100)) return false;

    auto const second = sut.observe(timestamp(1'201), report_period);
    if (second.quantized_timestamp() != timestamp(1'201)) return false;

    // 1'301.5 ns rounds to even integer 1'302.
    auto const third = sut.observe(timestamp(1'301), report_period);
    return third.quantized_timestamp() == timestamp(1'302);
}
static_assert(absolute_projection_rounds_fractional_time_for_kernel_api());

} // namespace
} // namespace crv
