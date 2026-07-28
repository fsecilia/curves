// SPDX-License-Identifier: MIT

#include "poll_period_tracker.hpp"
#include <crv/test/test.hpp>

namespace crv {
namespace {

using tracker_t = poll_period_tracker_t;
using timestamp_t = tracker_t::timestamp_t;
using period_t = tracker_t::period_t;
using period_error_t = tracker_t::period_error_t;
using status_t = tracker_t::status_t;

constexpr auto timestamp(uint64_t nanoseconds) noexcept -> timestamp_t
{
    return timestamp_t{nanoseconds};
}

constexpr auto period(uint64_t nanoseconds) noexcept -> period_t
{
    return period_t{nanoseconds};
}

constexpr auto raw_period(uint64_t value) noexcept -> period_t
{
    return period_t::literal(value);
}

constexpr auto error_period(uint64_t nanoseconds) noexcept -> period_error_t
{
    return period_error_t{static_cast<int128_t>(nanoseconds)};
}

constexpr auto default_params() noexcept -> tracker_t::params_t
{
    return {
        .initial_period = period(1'000),

        .measurement_window_ticks = 4,
        .maximum_training_gap_ticks = 16,

        .gain_shift = 3,

        .maximum_measurement_error = period(100),
        .maximum_correction = period(10),

        .minimum_period = period(500),
        .maximum_period = period(2'000),
    };
}

constexpr auto construction_uses_initial_period() noexcept -> bool
{
    auto const tracker = tracker_t{default_params()};

    return tracker.estimated_period() == period(1'000) && !tracker.anchored();
}
static_assert(construction_uses_initial_period());

constexpr auto reanchor_establishes_empty_window() noexcept -> bool
{
    auto tracker = tracker_t{default_params()};

    tracker.reanchor(timestamp(10'000));

    auto const update = tracker.observe(timestamp(11'000), 1);

    return tracker.anchored() && update.status == status_t::accumulating && update.estimated_period == period(1'000)
        && !update.has_measurement();
}
static_assert(reanchor_establishes_empty_window());

constexpr auto accumulates_below_threshold() noexcept -> bool
{
    auto tracker = tracker_t{default_params()};
    tracker.reanchor(timestamp(0));

    auto const first = tracker.observe(timestamp(1'000), 1);
    auto const second = tracker.observe(timestamp(2'000), 1);
    auto const third = tracker.observe(timestamp(3'000), 1);

    return first.status == status_t::accumulating && second.status == status_t::accumulating
        && third.status == status_t::accumulating && tracker.estimated_period() == period(1'000);
}
static_assert(accumulates_below_threshold());

constexpr auto updates_exactly_at_threshold() noexcept -> bool
{
    auto tracker = tracker_t{default_params()};
    tracker.reanchor(timestamp(0));

    static_cast<void>(tracker.observe(timestamp(1'000), 1));
    static_cast<void>(tracker.observe(timestamp(2'000), 1));
    static_cast<void>(tracker.observe(timestamp(3'000), 1));

    auto const update = tracker.observe(timestamp(4'000), 1);

    return update.status == status_t::updated && update.has_measurement() && update.measured_period == period(1'000)
        && update.correction == period_error_t{} && update.estimated_period == period(1'000);
}
static_assert(updates_exactly_at_threshold());

constexpr auto overshoot_uses_complete_observation() noexcept -> bool
{
    auto tracker = tracker_t{default_params()};
    tracker.reanchor(timestamp(0));

    auto const first = tracker.observe(timestamp(1'000), 1);

    // threshold is four ticks, but this takes the window to five
    auto const update = tracker.observe(timestamp(5'000), 4);

    // The four-tick observation takes the window from one tick to five. Measure the complete five-tick raw span; do not
    // synthesize a timestamp after the third tick and carry the fourth tick into the next window.
    auto const next = tracker.observe(timestamp(6'000), 1);

    return first.status == status_t::accumulating && update.status == status_t::updated
        && update.measured_period == period(1'000) && next.status == status_t::accumulating;
}
static_assert(overshoot_uses_complete_observation());

constexpr auto structured_distortion_cancels_over_window() noexcept -> bool
{
    auto tracker = tracker_t{default_params()};
    tracker.reanchor(timestamp(0));

    static_cast<void>(tracker.observe(timestamp(800), 1));
    static_cast<void>(tracker.observe(timestamp(2'000), 1));
    static_cast<void>(tracker.observe(timestamp(2'800), 1));

    auto const update = tracker.observe(timestamp(4'000), 1);

    return update.status == status_t::updated && update.measured_period == period(1'000)
        && update.correction == period_error_t{};
}
static_assert(structured_distortion_cancels_over_window());

constexpr auto hidden_reports_are_represented_by_tick_span() noexcept -> bool
{
    auto tracker = tracker_t{default_params()};
    tracker.reanchor(timestamp(0));

    auto const first = tracker.observe(timestamp(1'000), 1);

    // three elapsed polling ticks, only one delivered report
    auto const update = tracker.observe(timestamp(4'000), 3);

    return first.status == status_t::accumulating && update.status == status_t::updated
        && update.measured_period == period(1'000);
}
static_assert(hidden_reports_are_represented_by_tick_span());

constexpr auto converges_from_slightly_high_estimate() noexcept -> bool
{
    auto params = default_params();
    params.initial_period = period(1'008);

    auto tracker = tracker_t{params};
    tracker.reanchor(timestamp(0));

    auto const update = tracker.observe(timestamp(4'000), 4);

    return update.status == status_t::updated && update.measured_period == period(1'000)
        && update.correction == -error_period(1) && update.estimated_period == period(1'007);
}
static_assert(converges_from_slightly_high_estimate());

constexpr auto converges_from_slightly_low_estimate() noexcept -> bool
{
    auto params = default_params();
    params.initial_period = period(992);

    auto tracker = tracker_t{params};
    tracker.reanchor(timestamp(0));

    auto const update = tracker.observe(timestamp(4'000), 4);

    return update.status == status_t::updated && update.measured_period == period(1'000)
        && update.correction == error_period(1) && update.estimated_period == period(993);
}
static_assert(converges_from_slightly_low_estimate());

constexpr auto measures_fractional_q32_period() noexcept -> bool
{
    auto params = default_params();

    auto const expected = raw_period((uint64_t{1'000} << 32) + (uint64_t{1} << 31));

    params.initial_period = expected;
    params.measurement_window_ticks = 2;

    auto tracker = tracker_t{params};
    tracker.reanchor(timestamp(0));

    auto const update = tracker.observe(timestamp(2'001), 2);

    return update.status == status_t::updated && update.measured_period == expected
        && update.correction == period_error_t{};
}
static_assert(measures_fractional_q32_period());

constexpr auto period_division_rounds_ties_to_even() noexcept -> bool
{
    constexpr auto ticks = uint64_t{1} << 33;
    constexpr auto one_ns_raw = uint64_t{1} << 32;

    auto params = default_params();
    params.measurement_window_ticks = ticks;
    params.maximum_training_gap_ticks = ticks;
    params.gain_shift = 0;
    params.minimum_period = period(1);
    params.maximum_period = period(2);
    params.maximum_measurement_error = raw_period(4);
    params.maximum_correction = raw_period(4);

    // raw quotient:
    //
    //     (2^33 + 1)*2^32/2^33 = 2^32 + 0.5
    //
    // lower result is even, so tie rounds down
    params.initial_period = raw_period(one_ns_raw);

    auto even_tracker = tracker_t{params};
    even_tracker.reanchor(timestamp(0));

    auto const even_update = even_tracker.observe(timestamp(ticks + 1), ticks);

    // raw quotient:
    //
    //     (2^33 + 3)*2^32/2^33 = 2^32 + 1.5
    //
    // lower result is odd, so tie rounds up
    params.initial_period = raw_period(one_ns_raw + 2);

    auto odd_tracker = tracker_t{params};
    odd_tracker.reanchor(timestamp(0));

    auto const odd_update = odd_tracker.observe(timestamp(ticks + 3), ticks);

    return even_update.measured_period == raw_period(one_ns_raw)
        && odd_update.measured_period == raw_period(one_ns_raw + 2);
}
static_assert(period_division_rounds_ties_to_even());

constexpr auto signed_gain_shift_rounds_ties_to_even() noexcept -> bool
{
    constexpr auto ticks = uint64_t{1} << 32;
    constexpr auto base_raw = uint64_t{1'000} << 32;

    auto params = default_params();
    params.measurement_window_ticks = ticks;
    params.maximum_training_gap_ticks = ticks;
    params.gain_shift = 1;
    params.maximum_measurement_error = raw_period(16);
    params.maximum_correction = raw_period(16);
    params.minimum_period = period(900);
    params.maximum_period = period(1'100);

    // +0.5 raw unit -> 0 because zero is even
    params.initial_period = raw_period(base_raw);

    auto positive_even = tracker_t{params};
    positive_even.reanchor(timestamp(0));

    auto const positive_even_update = positive_even.observe(timestamp(base_raw + 1), ticks);

    // +1.5 raw units -> +2 because one is odd
    auto positive_odd = tracker_t{params};
    positive_odd.reanchor(timestamp(0));

    auto const positive_odd_update = positive_odd.observe(timestamp(base_raw + 3), ticks);

    // -0.5 raw unit -> 0
    params.initial_period = raw_period(base_raw + 1);

    auto negative_even = tracker_t{params};
    negative_even.reanchor(timestamp(0));

    auto const negative_even_update = negative_even.observe(timestamp(base_raw), ticks);

    // -1.5 raw units -> -2
    params.initial_period = raw_period(base_raw + 3);

    auto negative_odd = tracker_t{params};
    negative_odd.reanchor(timestamp(0));

    auto const negative_odd_update = negative_odd.observe(timestamp(base_raw), ticks);

    return positive_even_update.correction == period_error_t::literal(0)
        && positive_odd_update.correction == period_error_t::literal(2)
        && negative_even_update.correction == period_error_t::literal(0)
        && negative_odd_update.correction == period_error_t::literal(-2);
}
static_assert(signed_gain_shift_rounds_ties_to_even());

constexpr auto limits_maximum_correction() noexcept -> bool
{
    auto params = default_params();
    params.initial_period = period(1'100);
    params.gain_shift = 0;
    params.maximum_measurement_error = period(200);
    params.maximum_correction = period(10);

    auto tracker = tracker_t{params};
    tracker.reanchor(timestamp(0));

    auto const update = tracker.observe(timestamp(4'000), 4);

    return update.status == status_t::updated && update.correction == -error_period(10)
        && update.estimated_period == period(1'090);
}
static_assert(limits_maximum_correction());

constexpr auto limits_estimate_at_minimum_period() noexcept -> bool
{
    auto params = default_params();

    params.initial_period = period(505);
    params.measurement_window_ticks = 1;
    params.gain_shift = 0;
    params.maximum_measurement_error = period(10);
    params.maximum_correction = period(100);
    params.minimum_period = period(503);
    params.maximum_period = period(2'000);

    auto tracker = tracker_t{params};
    tracker.reanchor(timestamp(0));

    auto const update = tracker.observe(timestamp(500), 1);

    return update.status == status_t::updated && update.correction == -error_period(2)
        && update.estimated_period == period(503);
}
static_assert(limits_estimate_at_minimum_period());

constexpr auto limits_estimate_at_maximum_period() noexcept -> bool
{
    auto params = default_params();
    params.initial_period = period(1'495);
    params.measurement_window_ticks = 1;
    params.gain_shift = 0;
    params.maximum_measurement_error = period(10);
    params.maximum_correction = period(100);
    params.minimum_period = period(500);
    params.maximum_period = period(1'497);

    auto tracker = tracker_t{params};
    tracker.reanchor(timestamp(0));

    auto const update = tracker.observe(timestamp(1'500), 1);

    return update.status == status_t::updated && update.correction == error_period(2)
        && update.estimated_period == period(1'497);
}
static_assert(limits_estimate_at_maximum_period());

constexpr auto rejects_implausible_measurement() noexcept -> bool
{
    auto params = default_params();
    params.measurement_window_ticks = 1;
    params.maximum_measurement_error = period(100);

    auto tracker = tracker_t{params};
    tracker.reanchor(timestamp(0));

    auto const rejected = tracker.observe(timestamp(1'200), 1);

    // rejection reanchors at 1,200 ns; next observation measures a fresh 1,000 ns window
    auto const next = tracker.observe(timestamp(2'200), 1);

    return rejected.status == status_t::rejected && rejected.has_measurement()
        && rejected.measured_period == period(1'200) && rejected.correction == period_error_t{}
    && rejected.estimated_period == period(1'000) && next.status == status_t::updated
        && next.measured_period == period(1'000);
}
static_assert(rejects_implausible_measurement());

constexpr auto large_gap_discards_partial_window() noexcept -> bool
{
    auto tracker = tracker_t{default_params()};
    tracker.reanchor(timestamp(0));

    auto const partial = tracker.observe(timestamp(2'000), 2);
    auto const gap = tracker.observe(timestamp(20'000), 17);
    auto const next = tracker.observe(timestamp(24'000), 4);

    return partial.status == status_t::accumulating && gap.status == status_t::reanchored
        && gap.estimated_period == period(1'000) && next.status == status_t::updated
        && next.measured_period == period(1'000);
}
static_assert(large_gap_discards_partial_window());

constexpr auto explicit_reanchor_discards_partial_window() noexcept -> bool
{
    auto tracker = tracker_t{default_params()};
    tracker.reanchor(timestamp(0));

    static_cast<void>(tracker.observe(timestamp(2'000), 2));

    tracker.reanchor(timestamp(10'000));

    auto const update = tracker.observe(timestamp(14'000), 4);

    return update.status == status_t::updated && update.measured_period == period(1'000)
        && update.estimated_period == period(1'000);
}
static_assert(explicit_reanchor_discards_partial_window());

constexpr auto reset_restores_initial_state() noexcept -> bool
{
    auto params = default_params();
    params.initial_period = period(1'008);

    auto tracker = tracker_t{params};
    tracker.reanchor(timestamp(0));

    static_cast<void>(tracker.observe(timestamp(4'000), 4));

    if (tracker.estimated_period() != period(1'007)) { return false; }

    tracker.reset();

    return tracker.estimated_period() == period(1'008) && !tracker.anchored();
}
static_assert(reset_restores_initial_state());

constexpr auto initialization_tick_is_excluded_and_update_is_prospective() noexcept -> bool
{
    auto params = default_params();
    params.initial_period = period(1'008);

    auto tracker = tracker_t{params};
    auto quantizer = poll_interval_quantizer_t{};

    auto const initial_snapshot = tracker.estimated_period();
    auto const initial = quantizer.observe(timestamp(0), initial_snapshot);

    if (initial.status != poll_interval_quantizer_t::status_t::initialized) { return false; }

    tracker.reanchor(timestamp(0));

    auto const first_snapshot = tracker.estimated_period();
    auto const first = quantizer.observe(timestamp(1'000), first_snapshot);
    auto const first_update = tracker.observe(timestamp(1'000), first.elapsed_ticks);

    auto const second_snapshot = tracker.estimated_period();
    auto const second = quantizer.observe(timestamp(2'000), second_snapshot);
    auto const second_update = tracker.observe(timestamp(2'000), second.elapsed_ticks);

    auto const third_snapshot = tracker.estimated_period();
    auto const third = quantizer.observe(timestamp(3'000), third_snapshot);
    auto const third_update = tracker.observe(timestamp(3'000), third.elapsed_ticks);

    if (first_update.status != status_t::accumulating || second_update.status != status_t::accumulating
        || third_update.status != status_t::accumulating)
    {
        return false;
    }

    auto const fourth_snapshot = tracker.estimated_period();
    auto const fourth = quantizer.observe(timestamp(4'000), fourth_snapshot);
    auto const fourth_update = tracker.observe(timestamp(4'000), fourth.elapsed_ticks);

    if (fourth.report_period != period(1'008) || fourth_update.status != status_t::updated
        || fourth_update.estimated_period != period(1'007))
    {
        return false;
    }

    auto const fifth_snapshot = tracker.estimated_period();
    auto const fifth = quantizer.observe(timestamp(5'000), fifth_snapshot);

    return first.elapsed_ticks == 1 && second.elapsed_ticks == 1 && third.elapsed_ticks == 1
        && fourth.elapsed_ticks == 1 && fifth.report_period == period(1'007);
}
static_assert(initialization_tick_is_excluded_and_update_is_prospective());

} // namespace
} // namespace crv
