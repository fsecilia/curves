// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "poll_interval_quantizer.hpp"
#include <crv/test/test.hpp>
#include <array>

namespace crv {
namespace {

using sut_t = poll_interval_quantizer_t;
using timestamp_t = sut_t::timestamp_t;
using period_t = sut_t::period_t;
using residual_t = sut_t::residual_t;
using status_t = sut_t::status_t;

constexpr auto timestamp(uint64_t nanoseconds) noexcept -> timestamp_t
{
    return timestamp_t{nanoseconds};
}

constexpr auto period(uint64_t nanoseconds) noexcept -> period_t
{
    return period_t{nanoseconds};
}

constexpr auto residual(int64_t nanoseconds) noexcept -> residual_t
{
    return residual_t{nanoseconds};
}

constexpr auto initializes_without_inventing_hidden_ticks() noexcept -> bool
{
    auto sut = sut_t{period(100)};

    auto const result = sut.observe(timestamp(1'000));

    return result.report_period == period(100) && result.elapsed_ticks == 1 && result.hidden_zero_ticks() == 0
        && result.status == status_t::initialized && !result.minimum_tick_forced && sut.residual() == residual_t{}
    && sut.initialized();
}
static_assert(initializes_without_inventing_hidden_ticks());

constexpr auto exact_dense_stream_returns_one_tick() noexcept -> bool
{
    auto sut = sut_t{period(100)};

    static_cast<void>(sut.observe(timestamp(1'000)));
    auto const first = sut.observe(timestamp(1'100));
    auto const second = sut.observe(timestamp(1'200));

    return first.elapsed_ticks == 1 && second.elapsed_ticks == 1 && first.status == status_t::continuous
        && second.status == status_t::continuous && !first.minimum_tick_forced && !second.minimum_tick_forced
        && sut.residual() == residual_t{};
}
static_assert(exact_dense_stream_returns_one_tick());

constexpr auto exact_sparse_stream_reports_all_elapsed_ticks() noexcept -> bool
{
    auto sut = sut_t{period(100)};

    static_cast<void>(sut.observe(timestamp(1'000)));
    auto const result = sut.observe(timestamp(1'700));

    return result.elapsed_ticks == 7 && result.hidden_zero_ticks() == 6 && result.report_period == period(100)
        && !result.minimum_tick_forced && sut.residual() == residual_t{};
}
static_assert(exact_sparse_stream_reports_all_elapsed_ticks());

constexpr auto short_then_long_intervals_repay_timing_debt() noexcept -> bool
{
    auto sut = sut_t{period(100)};

    static_cast<void>(sut.observe(timestamp(1'000)));

    // 20 ns rounds to zero ticks, but a delivered report must consume one:
    //
    //     residual = 20 - 100 = -80 ns
    auto const short_result = sut.observe(timestamp(1'020));
    if (short_result.elapsed_ticks != 1) return false;
    if (!short_result.minimum_tick_forced) return false;
    if (sut.residual() != residual(-80)) return false;

    // The next raw interval is 180 ns:
    //
    //     corrected = 180 - 80 = 100 ns
    //
    // so it consumes one tick and repays the residual exactly.
    auto const long_result = sut.observe(timestamp(1'200));

    return long_result.elapsed_ticks == 1 && !long_result.minimum_tick_forced && sut.residual() == residual_t{};
}
static_assert(short_then_long_intervals_repay_timing_debt());

constexpr auto equal_timestamps_consume_one_tick_and_carry_debt() noexcept -> bool
{
    auto sut = sut_t{period(100)};

    static_cast<void>(sut.observe(timestamp(1'000)));

    auto const duplicate = sut.observe(timestamp(1'000));
    if (duplicate.elapsed_ticks != 1) return false;
    if (!duplicate.minimum_tick_forced) return false;
    if (sut.residual() != residual(-100)) return false;

    auto const repayment = sut.observe(timestamp(1'200));

    return repayment.elapsed_ticks == 1 && !repayment.minimum_tick_forced && sut.residual() == residual_t{};
}
static_assert(equal_timestamps_consume_one_tick_and_carry_debt());

constexpr auto half_tick_ties_round_to_even() noexcept -> bool
{
    {
        auto sut = sut_t{period(100)};
        static_cast<void>(sut.observe(timestamp(1'000)));

        // 2.5 ticks: lower integer 2 is even.
        auto const result = sut.observe(timestamp(1'250));

        if (result.elapsed_ticks != 2) return false;
        if (sut.residual() != residual(50)) return false;
    }

    {
        auto sut = sut_t{period(100)};
        static_cast<void>(sut.observe(timestamp(1'000)));

        // 3.5 ticks: lower integer 3 is odd, so round to 4.
        auto const result = sut.observe(timestamp(1'350));

        if (result.elapsed_ticks != 4) return false;
        if (sut.residual() != residual(-50)) return false;
    }

    return true;
}
static_assert(half_tick_ties_round_to_even());

constexpr auto half_tick_zero_tie_is_forced_to_one() noexcept -> bool
{
    auto sut = sut_t{period(100)};

    static_cast<void>(sut.observe(timestamp(1'000)));

    // Nearest-even would select zero because zero is even. The stream
    // invariant then forces one consumed tick.
    auto const result = sut.observe(timestamp(1'050));

    return result.elapsed_ticks == 1 && result.minimum_tick_forced && sut.residual() == residual(-50);
}
static_assert(half_tick_zero_tie_is_forced_to_one());

constexpr auto varying_sparse_intervals_preserve_tick_counts() noexcept -> bool
{
    constexpr auto expected_ticks = std::array<uint64_t, 5>{4, 7, 2, 11, 3};

    auto sut = sut_t{period(100)};
    auto current_timestamp = uint64_t{1'000};

    static_cast<void>(sut.observe(timestamp(current_timestamp)));

    for (auto const ticks : expected_ticks)
    {
        current_timestamp += ticks * 100;

        auto const result = sut.observe(timestamp(current_timestamp));
        if (result.elapsed_ticks != ticks) return false;
        if (result.minimum_tick_forced) return false;
        if (sut.residual() != residual_t{}) return false;
    }

    return true;
}
static_assert(varying_sparse_intervals_preserve_tick_counts());

constexpr auto timestamp_regression_is_visible_and_reanchors() noexcept -> bool
{
    auto sut = sut_t{period(100)};

    static_cast<void>(sut.observe(timestamp(1'000)));
    static_cast<void>(sut.observe(timestamp(1'020)));

    if (sut.residual() != residual(-80)) return false;

    auto const regression = sut.observe(timestamp(900));

    if (regression.status != status_t::timestamp_regressed) return false;
    if (regression.elapsed_ticks != 1) return false;
    if (regression.minimum_tick_forced) return false;
    if (sut.residual() != residual_t{}) return false;

    auto const recovered = sut.observe(timestamp(1'000));

    return recovered.status == status_t::continuous && recovered.elapsed_ticks == 1 && !recovered.minimum_tick_forced
        && sut.residual() == residual_t{};
}
static_assert(timestamp_regression_is_visible_and_reanchors());

constexpr auto reset_restores_uninitialized_state() noexcept -> bool
{
    auto sut = sut_t{period(100)};

    static_cast<void>(sut.observe(timestamp(1'000)));
    static_cast<void>(sut.observe(timestamp(1'020)));
    sut.reset();

    if (sut.initialized()) return false;
    if (sut.residual() != residual_t{}) return false;

    auto const result = sut.observe(timestamp(9'000));

    return result.status == status_t::initialized && result.elapsed_ticks == 1 && sut.residual() == residual_t{};
}
static_assert(reset_restores_uninitialized_state());

constexpr auto mixed_trace_conserves_elapsed_time() noexcept -> bool
{
    constexpr auto timestamps = std::array<uint64_t, 7>{
        1'000,
        1'020, // 20
        1'200, // 180
        1'500, // 300
        1'750, // 250
        1'760, // 10
        2'200, // 440
    };

    auto sut = sut_t{period(100)};

    auto observed_sum = residual_t{};
    auto quantized_sum = residual_t{};

    static_cast<void>(sut.observe(timestamp(timestamps.front())));

    for (auto index = std::size_t{1}; index < timestamps.size(); ++index)
    {
        auto const raw_delta = timestamp_t{timestamps[index]} - timestamp_t{timestamps[index - 1]};

        auto const result = sut.observe(timestamp(timestamps[index]));

        using tick_count_fixed_t = fixed_t<sut_t::tick_count_t, 0>;

        auto const quantized_interval
            = multiply(result.report_period, tick_count_fixed_t::literal(result.elapsed_ticks));

        observed_sum += residual_t::convert(raw_delta);
        quantized_sum += residual_t::convert(quantized_interval);
    }

    // Since the initial residual is zero:
    //
    //     observed elapsed = quantized elapsed + final residual
    return observed_sum == quantized_sum + sut.residual() && observed_sum == residual(1'200)
        && quantized_sum == residual(1'200) && sut.residual() == residual_t{};
}
static_assert(mixed_trace_conserves_elapsed_time());

TEST(poll_interval_quantizer_test, long_bunched_trace_conserves_elapsed_time)
{
    constexpr auto polling_period = uint64_t{250'000};

    auto sut = sut_t{period(polling_period)};

    // Deterministic compressed/expanded groups. Every four observed intervals
    // total exactly four polling periods.
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

    static_cast<void>(sut.observe(timestamp(current_timestamp)));

    for (auto repetition = 0; repetition < 10'000; ++repetition)
    {
        for (auto const raw_interval : interval_pattern)
        {
            current_timestamp += raw_interval;

            auto const result = sut.observe(timestamp(current_timestamp));

            using tick_count_fixed_t = fixed_t<sut_t::tick_count_t, 0>;

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

} // namespace
} // namespace crv
