// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "report_clock.hpp"
#include <crv/test/test.hpp>

namespace crv::pipeline::filters {

using sut_t = report_clock_t;
using timestamp_t = sut_t::timestamp_t;
using period_t = sut_t::period_t;
using gain_t = sut_t::gain_t;

constexpr auto timestamp(uint64_t nanoseconds) noexcept -> timestamp_t
{
    return timestamp_t::literal(nanoseconds);
}

constexpr auto period(uint64_t nanoseconds) noexcept -> period_t
{
    return period_t{nanoseconds};
}

constexpr auto gain_half = gain_t::literal(uint64_t{1} << 63);
constexpr auto gain_quarter = gain_t::literal(uint64_t{1} << 62);

constexpr auto test_params(gain_t phase_gain = gain_half, gain_t period_gain = gain_quarter,
    period_t minimum = period(50), period_t maximum = period(150)) noexcept -> sut_t::params_t
{
    return {
        .nominal_period = period(100),
        .phase_error_gain = phase_gain,
        .period_error_gain = period_gain,
        .minimum_period = minimum,
        .maximum_period = maximum,
        .gap_threshold_periods = 4,
    };
}

constexpr auto initializes_by_right_justifying_the_first_batch() noexcept -> bool
{
    auto sut = sut_t{test_params()};
    auto const timing = sut(timestamp(1'000), 3);

    return timing.continuity == sut_t::continuity_t::initial && timing.report_period == period(100)
        && timing.report_timestamp(0, 3) == timestamp(800) && timing.report_timestamp(1, 3) == timestamp(900)
        && timing.report_timestamp(2, 3) == timestamp(1'000) && timing.preceding_zero_timestamp(3) == timestamp(700)
        && sut.next_report_time() == sut_t::recovered_time_t{1'100};
}
static_assert(initializes_by_right_justifying_the_first_batch());

constexpr auto advances_prediction_across_every_report_in_a_batch() noexcept -> bool
{
    auto sut = sut_t{test_params()};
    static_cast<void>(sut(timestamp(1'000), 1));

    auto const timing = sut(timestamp(1'300), 3);
    return timing.continuity == sut_t::continuity_t::contiguous && timing.report_timestamp(0, 3) == timestamp(1'100)
        && timing.report_timestamp(1, 3) == timestamp(1'200) && timing.report_timestamp(2, 3) == timestamp(1'300)
        && sut.estimated_period() == period(100) && sut.next_report_time() == sut_t::recovered_time_t{1'400};
}
static_assert(advances_prediction_across_every_report_in_a_batch());

constexpr auto correction_applies_to_future_reports_only() noexcept -> bool
{
    auto sut = sut_t{test_params()};
    static_cast<void>(sut(timestamp(1'000), 1));

    // predicted = 1100, observed = 1150, error = 50
    // phase correction  = 1/2 * 50 = 25
    // period correction = 1/4 * 50 = 12.5
    auto const timing = sut(timestamp(1'150), 1);

    auto const twelve_and_a_half_raw = uint64_t{12} << 32 | uint64_t{1} << 31;
    auto const expected_period = period_t::literal(period(100).value + twelve_and_a_half_raw);

    return timing.continuity == sut_t::continuity_t::contiguous && timing.report_timestamp(0, 1) == timestamp(1'100)
        && timing.report_period == period(100) && sut.estimated_period() == expected_period
        && sut.next_report_time() == sut_t::recovered_time_t{1'225};
}
static_assert(correction_applies_to_future_reports_only());

constexpr auto gap_right_justifies_and_preserves_period() noexcept -> bool
{
    auto sut = sut_t{test_params()};
    static_cast<void>(sut(timestamp(1'000), 1));

    // predicted = 1100; 1501 is more than four periods late.
    auto const timing = sut(timestamp(1'501), 1);

    return timing.continuity == sut_t::continuity_t::gap && timing.report_timestamp(0, 1) == timestamp(1'501)
        && timing.report_period == period(100) && sut.estimated_period() == period(100)
        && sut.next_report_time() == sut_t::recovered_time_t{1'601};
}
static_assert(gap_right_justifies_and_preserves_period());

constexpr auto exactly_the_gap_threshold_is_still_contiguous() noexcept -> bool
{
    auto sut = sut_t{test_params()};
    static_cast<void>(sut(timestamp(1'000), 1));

    // predicted = 1100; error = 400 exactly.
    auto const timing = sut(timestamp(1'500), 1);
    return timing.continuity != sut_t::continuity_t::gap;
}
static_assert(exactly_the_gap_threshold_is_still_contiguous());

constexpr auto equal_observations_do_not_reset_the_clock() noexcept -> bool
{
    auto sut = sut_t{test_params()};

    static_cast<void>(sut(timestamp(1'000), 1));
    auto const timing = sut(timestamp(1'000), 1);

    return timing.continuity == sut_t::continuity_t::contiguous;
}
static_assert(equal_observations_do_not_reset_the_clock());

constexpr auto backward_observation_resets_the_clock() noexcept -> bool
{
    auto sut = sut_t{test_params()};

    static_cast<void>(sut(timestamp(1'000), 1));
    static_cast<void>(sut(timestamp(1'100), 1));

    auto const timing = sut(timestamp(1'099), 1);

    return timing.continuity == sut_t::continuity_t::clock_reset && timing.report_timestamp(0, 1) == timestamp(1'099)
        && timing.report_period == period(100) && sut.estimated_period() == period(100)
        && sut.next_report_time() == sut_t::recovered_time_t{1'199} && sut.initialized();
}
static_assert(backward_observation_resets_the_clock());

constexpr auto period_correction_clamps_to_the_configured_envelope() noexcept -> bool
{
    auto sut = sut_t{test_params(gain_half, gain_half, period(95), period(105))};
    static_cast<void>(sut(timestamp(1'000), 1));

    // error = 200, requested period correction = +100 -> clamp to 105.
    static_cast<void>(sut(timestamp(1'300), 1));
    return sut.estimated_period() == period(105);
}
static_assert(period_correction_clamps_to_the_configured_envelope());

constexpr auto default_8khz_coefficients_are_stable() noexcept -> bool
{
    auto const params = sut_t::default_params(period(125'000));

    // Derived from the Q0.64 constants in default_params, not decimal
    // re-evaluation in the kernel.
    return params.phase_error_gain.value == 10'244'590'563'750'000ULL
        && params.period_error_gain.value == 2'844'719'789'032ULL
        && params.minimum_period.value == period(125'000).value - period(125'000).value / 20
        && params.maximum_period.value == period(125'000).value + period(125'000).value / 20
        && params.gap_threshold_periods == 4;
}
static_assert(default_8khz_coefficients_are_stable());

constexpr auto fractional_period_is_error_diffused_into_integral_timestamps() noexcept -> bool
{
    auto params = test_params();
    params.nominal_period = period_t::literal((uint64_t{100} << 32) + (uint64_t{1} << 31)); // 100.5 ns
    params.minimum_period = params.nominal_period;
    params.maximum_period = params.nominal_period;
    params.phase_error_gain = {};
    params.period_error_gain = {};

    auto sut = sut_t{params};
    auto const timing = sut(timestamp(1'000), 3);

    // Exact times are 799.0, 899.5, and 1000.0 ns. Nearest-even emits
    // 799, 900, 1000, preserving the 201 ns span over two reports.
    return timing.report_timestamp(0, 3) == timestamp(799) && timing.report_timestamp(1, 3) == timestamp(900)
        && timing.report_timestamp(2, 3) == timestamp(1'000);
}
static_assert(fractional_period_is_error_diffused_into_integral_timestamps());

} // namespace crv::pipeline::filters
