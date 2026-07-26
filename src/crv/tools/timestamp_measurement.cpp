// SPDX-License-Identifier: MIT

/// \file
/// \brief reports timestamp statistics from a raw input-value capture
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/filter/poll_interval_quantizer.hpp>
#include <crv/io/capture/file.hpp>
#include <crv/io/capture/stream.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/io.hpp>

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cmath>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace crv {
namespace {

auto volatile stop_requested = std::sig_atomic_t{0};

extern "C" void stop_signal_handler(int)
{
    stop_requested = 1;
}

[[nodiscard]] auto install_signal_handlers() -> bool
{
    struct sigaction action{};
    action.sa_handler = stop_signal_handler;
    sigemptyset(&action.sa_mask);

    // Deliberately omit SA_RESTART so interruption is visible to the decoder.
    action.sa_flags = 0;

    return sigaction(SIGINT, &action, nullptr) == 0 && sigaction(SIGTERM, &action, nullptr) == 0;
}

[[nodiscard]] constexpr auto capture_stream_error_name(capture_stream_error_code_t code) noexcept -> char const*
{
    switch (code)
    {
        case capture_stream_error_code_t::interrupted: return "interrupted";
        case capture_stream_error_code_t::source_disconnected: return "source disconnected";
        case capture_stream_error_code_t::source_read_failed: return "source read failed";

        case capture_stream_error_code_t::truncated_stream_header: return "truncated stream header";
        case capture_stream_error_code_t::invalid_magic: return "invalid stream magic";
        case capture_stream_error_code_t::unsupported_format_version: return "unsupported format version";
        case capture_stream_error_code_t::invalid_stream_header_size: return "invalid stream-header size";
        case capture_stream_error_code_t::stream_header_too_large: return "stream header exceeds decoder limit";
        case capture_stream_error_code_t::unsupported_input_value_size: return "unsupported input-value size";
        case capture_stream_error_code_t::unsupported_clock: return "unsupported capture clock";
        case capture_stream_error_code_t::unsupported_byte_order: return "unsupported byte order";
        case capture_stream_error_code_t::unsupported_stream_flags: return "unsupported stream flags";

        case capture_stream_error_code_t::truncated_frame: return "truncated frame";
        case capture_stream_error_code_t::invalid_frame_size: return "invalid frame size";
        case capture_stream_error_code_t::invalid_frame_header_size: return "invalid frame-header size";
        case capture_stream_error_code_t::frame_too_large: return "frame exceeds decoder limit";

        case capture_stream_error_code_t::invalid_input_values_header_size: return "invalid input-values header size";

        case capture_stream_error_code_t::invalid_input_value_count: return "invalid input-value count";

        case capture_stream_error_code_t::invalid_input_value_capacity: return "invalid input-value capacity";

        case capture_stream_error_code_t::input_value_capacity_too_large:
            return "input-value capacity exceeds decoder limit";

        case capture_stream_error_code_t::inconsistent_input_values_frame_size:
            return "input-values frame size is inconsistent";
    }

    return "unknown capture-stream error";
}

void print_decode_error(char const* path, capture_stream_error_t const& error)
{
    std::fprintf(stderr, "%s: decode failed at byte %llu: %s", path,
        static_cast<unsigned long long>(error.stream_offset), capture_stream_error_name(error.code));

    if (error.system_error != 0) { std::fprintf(stderr, ": %s", std::strerror(error.system_error)); }

    std::fputc('\n', stderr);
}

[[nodiscard]] auto parse_expected_rate(char const* text) -> std::optional<long double>
{
    errno = 0;

    auto* end = static_cast<char*>(nullptr);
    auto const value = std::strtold(text, &end);

    if (end == text || *end != '\0' || errno == ERANGE || !std::isfinite(value) || value <= 0.0L)
    {
        return std::nullopt;
    }

    // A nanosecond timestamp cannot distinguish a period below one nanosecond.
    if (value > 1'000'000'000.0L) return std::nullopt;

    return value;
}

struct timestamp_pair_t
{
    std::uint64_t previous_sequence;
    std::uint64_t sequence;
    std::uint64_t previous_timestamp_ns;
    std::uint64_t timestamp_ns;
    std::uint64_t delta_ns;
};

struct timestamp_regression_t
{
    std::uint64_t previous_sequence;
    std::uint64_t sequence;
    std::uint64_t previous_timestamp_ns;
    std::uint64_t timestamp_ns;
};

struct sequence_discontinuity_t
{
    std::uint64_t previous_sequence;
    std::uint64_t expected_sequence;
    std::uint64_t received_sequence;
};

[[nodiscard]] auto percentage(std::uint64_t count, std::size_t total) noexcept -> long double
{
    if (total == 0) return 0.0L;

    return 100.0L * static_cast<long double>(count) / static_cast<long double>(total);
}

void print_count(char const* label, std::uint64_t count, std::size_t total)
{
    std::fprintf(
        stdout, "  %-37s %12llu  %9.5Lf%%\n", label, static_cast<unsigned long long>(count), percentage(count, total));
}

void print_duration(char const* label, long double nanoseconds)
{
    std::fprintf(stdout, "  %-21s %15.3Lf us  (%0.3Lf ns)\n", label, nanoseconds / 1'000.0L, nanoseconds);
}

[[nodiscard]] auto quantile(std::vector<std::uint64_t> const& sorted, long double probability) -> long double
{
    assert(!sorted.empty());
    assert(probability >= 0.0L);
    assert(probability <= 1.0L);

    auto const position = probability * static_cast<long double>(sorted.size() - 1);
    auto const lower_index = static_cast<std::size_t>(position);
    auto const upper_index = std::min(lower_index + 1, sorted.size() - 1);
    auto const fraction = position - static_cast<long double>(lower_index);

    auto const lower = static_cast<long double>(sorted[lower_index]);
    auto const upper = static_cast<long double>(sorted[upper_index]);

    return lower + fraction * (upper - lower);
}

class timestamp_stats_t
{
private:
    static constexpr auto histogram_bin_width_ns = std::uint64_t{5'000};
    static constexpr auto histogram_limit_ns = std::uint64_t{1'000'000};
    static constexpr auto histogram_bin_count = static_cast<std::size_t>(histogram_limit_ns / histogram_bin_width_ns);

    static constexpr auto displayed_histogram_bin_count = std::size_t{24};
    static constexpr auto shortest_delta_context_count = std::size_t{16};

    // At 4 kHz this is ±31.25 us around the expected aggregate interval.
    static constexpr auto compensation_tolerance_fraction = 0.125L;

    struct delta_context_t
    {
        timestamp_pair_t sample;
        std::optional<std::uint64_t> previous_delta_ns;
        std::optional<std::uint64_t> next_delta_ns;
    };

public:
    explicit timestamp_stats_t(long double expected_rate_hz)
        : expected_rate_hz_{expected_rate_hz}, histogram_counts_(histogram_bin_count, 0)
    {
        expected_period_ns_ = 1'000'000'000.0L / expected_rate_hz_;
    }

    auto observe(capture_input_values_view_t const& frame) -> void
    {
        ++frame_count_;

        if (frame.values.empty()) { ++empty_frame_count_; }
        else
        {
            auto syn_report_index = std::size_t{0};
            while (syn_report_index < frame.values.size()
                && (frame.values[syn_report_index].type != CRV_EV_SYN
                    || frame.values[syn_report_index].code != CRV_SYN_REPORT))
            {
                ++syn_report_index;
            }
            if (syn_report_index == frame.values.size()) ++frames_missing_syn_report_count_;
            else
            {
                if (syn_report_index != frame.values.size() - 1) ++frames_misplaced_syn_report_count_;
                if (frame.values[syn_report_index].value != 0) ++split_frames_count_;
            }
        }

        if (!initialized_)
        {
            initialized_ = true;

            first_sequence_ = frame.sequence;
            last_sequence_ = frame.sequence;

            first_timestamp_ns_ = frame.timestamp_ns;
            last_timestamp_ns_ = frame.timestamp_ns;
            minimum_timestamp_ns_ = frame.timestamp_ns;
            maximum_timestamp_ns_ = frame.timestamp_ns;

            return;
        }

        minimum_timestamp_ns_ = std::min(minimum_timestamp_ns_, frame.timestamp_ns);
        maximum_timestamp_ns_ = std::max(maximum_timestamp_ns_, frame.timestamp_ns);

        auto const expected_sequence = last_sequence_ + 1;
        auto const sequence_is_contiguous = frame.sequence == expected_sequence;

        if (!sequence_is_contiguous)
        {
            ++sequence_discontinuity_count_;

            if (!first_sequence_discontinuity_)
            {
                first_sequence_discontinuity_ = sequence_discontinuity_t{
                    .previous_sequence = last_sequence_,
                    .expected_sequence = expected_sequence,
                    .received_sequence = frame.sequence,
                };
            }

            if (frame.sequence > expected_sequence)
            {
                auto const missing = frame.sequence - expected_sequence;

                if (missing > std::numeric_limits<std::uint64_t>::max() - missing_sequence_count_)
                {
                    missing_sequence_count_overflowed_ = true;
                }
                else if (!missing_sequence_count_overflowed_) { missing_sequence_count_ += missing; }
            }
            else
            {
                ++nonforward_sequence_count_;
            }

            break_delta_chain();
        }
        else if (frame.timestamp_ns < last_timestamp_ns_)
        {
            ++timestamp_regression_count_;

            if (!first_timestamp_regression_)
            {
                first_timestamp_regression_ = timestamp_regression_t{
                    .previous_sequence = last_sequence_,
                    .sequence = frame.sequence,
                    .previous_timestamp_ns = last_timestamp_ns_,
                    .timestamp_ns = frame.timestamp_ns,
                };
            }

            break_delta_chain();
        }
        else
        {
            observe_delta(timestamp_pair_t{
                .previous_sequence = last_sequence_,
                .sequence = frame.sequence,
                .previous_timestamp_ns = last_timestamp_ns_,
                .timestamp_ns = frame.timestamp_ns,
                .delta_ns = frame.timestamp_ns - last_timestamp_ns_,
            });
        }

        last_sequence_ = frame.sequence;
        last_timestamp_ns_ = frame.timestamp_ns;
    }

    auto print() -> void
    {
        // The final delta has no known successor until input ends.
        break_delta_chain();

        std::fprintf(stdout, "capture\n");
        std::fprintf(
            stdout, "  frames                                %12llu\n", static_cast<unsigned long long>(frame_count_));
        std::fprintf(stdout, "  empty frames                          %12llu\n",
            static_cast<unsigned long long>(empty_frame_count_));
        std::fprintf(stdout, "  frames missing syn report             %12llu\n",
            static_cast<unsigned long long>(frames_missing_syn_report_count_));
        std::fprintf(stdout, "  frames with misplaced syn report      %12llu\n",
            static_cast<unsigned long long>(frames_misplaced_syn_report_count_));
        std::fprintf(stdout, "  split frames                          %12llu\n",
            static_cast<unsigned long long>(split_frames_count_));

        if (!initialized_)
        {
            std::fprintf(stdout, "  no input-values frames were present\n");
            return;
        }

        std::fprintf(stdout, "  first sequence                        %12llu\n",
            static_cast<unsigned long long>(first_sequence_));

        std::fprintf(stdout, "  last sequence                         %12llu\n",
            static_cast<unsigned long long>(last_sequence_));

        std::fprintf(stdout, "  first timestamp                       %12llu ns\n",
            static_cast<unsigned long long>(first_timestamp_ns_));

        std::fprintf(stdout, "  last timestamp                        %12llu ns\n",
            static_cast<unsigned long long>(last_timestamp_ns_));

        print_duration("timestamp range", static_cast<long double>(maximum_timestamp_ns_ - minimum_timestamp_ns_));

        auto const adjacent_pair_count
            = frame_count_ == 0 ? std::size_t{0} : static_cast<std::size_t>(frame_count_ - 1);

        std::fprintf(stdout, "\nsequence integrity\n");

        print_count("sequence discontinuities", sequence_discontinuity_count_, adjacent_pair_count);

        print_count("nonforward or duplicate sequences", nonforward_sequence_count_, adjacent_pair_count);

        if (missing_sequence_count_overflowed_)
        {
            std::fprintf(stdout, "  %-37s %12s\n", "missing forward sequences", "overflow");
        }
        else
        {
            std::fprintf(stdout, "  %-37s %12llu\n", "missing forward sequences",
                static_cast<unsigned long long>(missing_sequence_count_));
        }

        std::fprintf(stdout, "\ntimestamp integrity\n");

        print_count("timestamp regressions", timestamp_regression_count_, adjacent_pair_count);

        print_count("usable contiguous deltas", static_cast<std::uint64_t>(deltas_ns_.size()), adjacent_pair_count);

        print_count("same-timestamp contiguous pairs", same_timestamp_count_, deltas_ns_.size());

        if (deltas_ns_.empty())
        {
            std::fprintf(stdout, "\nno valid contiguous timestamp deltas were available\n");
            print_integrity_examples();
            return;
        }

        std::sort(deltas_ns_.begin(), deltas_ns_.end());

        std::fprintf(stdout, "\ncontiguous delta distribution\n");

        print_duration("minimum", quantile(deltas_ns_, 0.0L));

        if (minimum_positive_delta_)
        {
            print_duration("minimum positive", static_cast<long double>(minimum_positive_delta_->delta_ns));
        }
        else
        {
            std::fprintf(stdout, "  %-21s %15s\n", "minimum positive", "none");
        }

        print_duration("0.1th percentile", quantile(deltas_ns_, 0.001L));
        print_duration("1st percentile", quantile(deltas_ns_, 0.01L));
        print_duration("5th percentile", quantile(deltas_ns_, 0.05L));
        print_duration("median", quantile(deltas_ns_, 0.50L));
        print_duration("95th percentile", quantile(deltas_ns_, 0.95L));
        print_duration("99th percentile", quantile(deltas_ns_, 0.99L));
        print_duration("99.9th percentile", quantile(deltas_ns_, 0.999L));
        print_duration("maximum", quantile(deltas_ns_, 1.0L));

        print_duration("mean", delta_sum_ns_ / static_cast<long double>(deltas_ns_.size()));

        std::fprintf(stdout, "\nexpected polling rate\n");
        std::fprintf(stdout, "  %-37s %12.3Lf Hz\n", "configured rate", expected_rate_hz_);
        print_duration("expected period", expected_period_ns_);
        print_count("delta < 25% expected period", below_quarter_period_count_, deltas_ns_.size());
        print_count("delta < 50% expected period", below_half_period_count_, deltas_ns_.size());
        print_count("delta < 75% expected period", below_three_quarters_period_count_, deltas_ns_.size());
        print_short_delta_compensation();

        print_histogram_peaks();
        print_shortest_delta_contexts();
        print_integrity_examples();
    }

private:
    auto observe_delta(timestamp_pair_t sample) -> void
    {
        deltas_ns_.push_back(sample.delta_ns);
        delta_sum_ns_ += static_cast<long double>(sample.delta_ns);

        if (!minimum_delta_ || sample.delta_ns < minimum_delta_->delta_ns) { minimum_delta_ = sample; }

        if (sample.delta_ns == 0) { ++same_timestamp_count_; }
        else if (!minimum_positive_delta_ || sample.delta_ns < minimum_positive_delta_->delta_ns)
        {
            minimum_positive_delta_ = sample;
        }

        if (sample.delta_ns < histogram_limit_ns)
        {
            auto const index = static_cast<std::size_t>(sample.delta_ns / histogram_bin_width_ns);

            ++histogram_counts_[index];
        }
        else
        {
            ++histogram_overflow_count_;
        }

        auto const delta = static_cast<long double>(sample.delta_ns);
        if (delta < expected_period_ns_ * 0.25L) ++below_quarter_period_count_;
        if (delta < expected_period_ns_ * 0.50L) ++below_half_period_count_;
        if (delta < expected_period_ns_ * 0.75L) ++below_three_quarters_period_count_;

        auto previous_delta_ns = std::optional<std::uint64_t>{};

        if (pending_delta_)
        {
            /*
                A pending context can only survive when this delta immediately follows it. Gaps and regressions call
                break_delta_chain().
            */
            assert(pending_delta_->sample.sequence == sample.previous_sequence);

            previous_delta_ns = pending_delta_->sample.delta_ns;
            pending_delta_->next_delta_ns = sample.delta_ns;

            finalize_pending_delta();
        }

        pending_delta_ = delta_context_t{
            .sample = sample,
            .previous_delta_ns = previous_delta_ns,
            .next_delta_ns = std::nullopt,
        };
    }

    auto break_delta_chain() -> void
    {
        if (pending_delta_) finalize_pending_delta();
    }

    auto finalize_pending_delta() -> void
    {
        assert(pending_delta_);

        auto context = std::move(*pending_delta_);
        pending_delta_.reset();

        observe_compensation(context);
        retain_shortest_context(std::move(context));
    }

    auto observe_compensation(delta_context_t const& context) -> void
    {
        auto const delta = static_cast<long double>(context.sample.delta_ns);
        auto const short_threshold = expected_period_ns_ * 0.50L;

        if (delta >= short_threshold) return;

        ++short_delta_context_count_;

        auto previous_compensates = false;
        auto next_compensates = false;

        if (context.previous_delta_ns)
        {
            ++short_delta_with_previous_count_;

            auto const pair = static_cast<long double>(*context.previous_delta_ns) + delta;

            previous_compensates = approximately(pair, expected_period_ns_ * 2.0L);

            if (previous_compensates) { ++previous_plus_short_compensation_count_; }
        }

        if (context.next_delta_ns)
        {
            ++short_delta_with_next_count_;

            auto const pair = delta + static_cast<long double>(*context.next_delta_ns);

            next_compensates = approximately(pair, expected_period_ns_ * 2.0L);

            if (next_compensates) { ++short_plus_next_compensation_count_; }
        }

        if (previous_compensates || next_compensates) { ++either_pair_compensation_count_; }

        if (previous_compensates && next_compensates) { ++both_pair_compensation_count_; }

        if (context.previous_delta_ns && context.next_delta_ns)
        {
            ++short_delta_with_both_neighbors_count_;

            auto const triplet = static_cast<long double>(*context.previous_delta_ns) + delta
                + static_cast<long double>(*context.next_delta_ns);

            if (approximately(triplet, expected_period_ns_ * 3.0L)) { ++triplet_compensation_count_; }
        }
    }

    [[nodiscard]] auto approximately(long double value, long double target) const noexcept -> bool
    {
        auto const tolerance = expected_period_ns_ * compensation_tolerance_fraction;

        return std::fabs(value - target) <= tolerance;
    }

    auto retain_shortest_context(delta_context_t context) -> void
    {
        if (shortest_delta_contexts_.size() < shortest_delta_context_count)
        {
            shortest_delta_contexts_.push_back(std::move(context));
            return;
        }

        auto const longest = std::max_element(shortest_delta_contexts_.begin(), shortest_delta_contexts_.end(),
            [](delta_context_t const& left, delta_context_t const& right) {
                if (left.sample.delta_ns != right.sample.delta_ns)
                {
                    return left.sample.delta_ns < right.sample.delta_ns;
                }

                return left.sample.sequence < right.sample.sequence;
            });

        assert(longest != shortest_delta_contexts_.end());

        if (context.sample.delta_ns < longest->sample.delta_ns) { *longest = std::move(context); }
    }

    auto print_short_delta_compensation() const -> void
    {
        auto const short_threshold = expected_period_ns_ * 0.50L;
        auto const pair_target = expected_period_ns_ * 2.0L;
        auto const triplet_target = expected_period_ns_ * 3.0L;
        auto const tolerance = expected_period_ns_ * compensation_tolerance_fraction;

        std::fprintf(stdout, "\nshort-delta neighborhood compensation\n");

        print_duration("short threshold", short_threshold);
        print_duration("pair target", pair_target);
        print_duration("triplet target", triplet_target);
        print_duration("target tolerance", tolerance);

        print_count(
            "short deltas with previous neighbor", short_delta_with_previous_count_, short_delta_context_count_);

        print_count("short deltas with next neighbor", short_delta_with_next_count_, short_delta_context_count_);

        print_count(
            "short deltas with both neighbors", short_delta_with_both_neighbors_count_, short_delta_context_count_);

        print_count("previous + short near 2 periods", previous_plus_short_compensation_count_,
            short_delta_with_previous_count_);

        print_count("short + next near 2 periods", short_plus_next_compensation_count_, short_delta_with_next_count_);

        print_count("either adjacent pair near 2 periods", either_pair_compensation_count_, short_delta_context_count_);

        print_count("both adjacent pairs near 2 periods", both_pair_compensation_count_,
            short_delta_with_both_neighbors_count_);

        print_count(
            "three-delta window near 3 periods", triplet_compensation_count_, short_delta_with_both_neighbors_count_);
    }

    auto print_histogram_peaks() const -> void
    {
        auto populated_bins = std::vector<std::size_t>{};
        populated_bins.reserve(histogram_counts_.size());

        for (auto index = std::size_t{0}; index < histogram_counts_.size(); ++index)
        {
            if (histogram_counts_[index] != 0) { populated_bins.push_back(index); }
        }

        std::sort(populated_bins.begin(), populated_bins.end(), [this](std::size_t left, std::size_t right) {
            if (histogram_counts_[left] != histogram_counts_[right])
            {
                return histogram_counts_[left] > histogram_counts_[right];
            }

            return left < right;
        });

        if (populated_bins.size() > displayed_histogram_bin_count)
        {
            populated_bins.resize(displayed_histogram_bin_count);
        }

        // Select by density, but display in timestamp order.
        std::sort(populated_bins.begin(), populated_bins.end());

        std::fprintf(stdout,
            "\ndensest 5 us bins below 1 ms "
            "(top %zu bins, displayed in time order)\n",
            displayed_histogram_bin_count);

        std::fprintf(stdout, "  %-23s %12s %11s\n", "delta range", "count", "usable");

        for (auto const index : populated_bins)
        {
            auto const lower_ns = static_cast<std::uint64_t>(index) * histogram_bin_width_ns;

            auto const upper_ns = lower_ns + histogram_bin_width_ns;
            auto const count = histogram_counts_[index];

            std::fprintf(stdout, "  %8.3Lf - %8.3Lf us %12llu %10.5Lf%%\n",
                static_cast<long double>(lower_ns) / 1'000.0L, static_cast<long double>(upper_ns) / 1'000.0L,
                static_cast<unsigned long long>(count), percentage(count, deltas_ns_.size()));
        }

        print_count("delta >= 1 ms", histogram_overflow_count_, deltas_ns_.size());
    }

    auto print_shortest_delta_contexts() const -> void
    {
        auto contexts = shortest_delta_contexts_;

        std::sort(contexts.begin(), contexts.end(), [](delta_context_t const& left, delta_context_t const& right) {
            if (left.sample.delta_ns != right.sample.delta_ns) { return left.sample.delta_ns < right.sample.delta_ns; }

            return left.sample.sequence < right.sample.sequence;
        });

        std::fprintf(stdout, "\nshortest contiguous deltas with immediate neighbors\n");

        std::fprintf(stdout, "  %-29s %12s %12s %12s %16s %16s\n", "sequence", "previous us", "delta us", "next us",
            "previous+delta", "delta+next");

        for (auto const& context : contexts)
        {
            char previous_buffer[32]{};
            char next_buffer[32]{};
            char previous_sum_buffer[32]{};
            char next_sum_buffer[32]{};
            char sequence_buffer[64]{};

            format_optional_duration(context.previous_delta_ns
                    ? std::optional<long double>{static_cast<long double>(*context.previous_delta_ns)}
                    : std::nullopt,
                previous_buffer, sizeof(previous_buffer));

            format_optional_duration(context.next_delta_ns
                    ? std::optional<long double>{static_cast<long double>(*context.next_delta_ns)}
                    : std::nullopt,
                next_buffer, sizeof(next_buffer));

            format_optional_duration(context.previous_delta_ns
                    ? std::optional<long double>{static_cast<long double>(*context.previous_delta_ns)
                          + static_cast<long double>(context.sample.delta_ns)}
                    : std::nullopt,
                previous_sum_buffer, sizeof(previous_sum_buffer));

            format_optional_duration(context.next_delta_ns
                    ? std::optional<long double>{static_cast<long double>(context.sample.delta_ns)
                          + static_cast<long double>(*context.next_delta_ns)}
                    : std::nullopt,
                next_sum_buffer, sizeof(next_sum_buffer));

            std::snprintf(sequence_buffer, sizeof(sequence_buffer), "%llu -> %llu",
                static_cast<unsigned long long>(context.sample.previous_sequence),
                static_cast<unsigned long long>(context.sample.sequence));

            std::fprintf(stdout, "  %-29s %12s %12.3Lf %12s %16s %16s\n", sequence_buffer, previous_buffer,
                static_cast<long double>(context.sample.delta_ns) / 1'000.0L, next_buffer, previous_sum_buffer,
                next_sum_buffer);
        }
    }

    static auto format_optional_duration(
        std::optional<long double> nanoseconds, char* destination, std::size_t destination_size) -> void
    {
        assert(destination != nullptr);
        assert(destination_size != 0);

        if (!nanoseconds)
        {
            std::snprintf(destination, destination_size, "-");
            return;
        }

        std::snprintf(destination, destination_size, "%.3Lf", *nanoseconds / 1'000.0L);
    }

    auto print_integrity_examples() const -> void
    {
        if (!first_timestamp_regression_ && !first_sequence_discontinuity_) { return; }

        std::fprintf(stdout, "\nintegrity failure examples\n");

        if (first_timestamp_regression_)
        {
            auto const amount
                = first_timestamp_regression_->previous_timestamp_ns - first_timestamp_regression_->timestamp_ns;

            std::fprintf(stdout,
                "  first timestamp regression: sequence %llu -> %llu, "
                "timestamp %llu -> %llu, backward by %llu ns\n",
                static_cast<unsigned long long>(first_timestamp_regression_->previous_sequence),
                static_cast<unsigned long long>(first_timestamp_regression_->sequence),
                static_cast<unsigned long long>(first_timestamp_regression_->previous_timestamp_ns),
                static_cast<unsigned long long>(first_timestamp_regression_->timestamp_ns),
                static_cast<unsigned long long>(amount));
        }

        if (first_sequence_discontinuity_)
        {
            std::fprintf(stdout,
                "  first sequence discontinuity: previous %llu, "
                "expected %llu, received %llu\n",
                static_cast<unsigned long long>(first_sequence_discontinuity_->previous_sequence),
                static_cast<unsigned long long>(first_sequence_discontinuity_->expected_sequence),
                static_cast<unsigned long long>(first_sequence_discontinuity_->received_sequence));
        }
    }

    long double expected_rate_hz_;
    long double expected_period_ns_;

    bool initialized_ = false;
    std::uint64_t frame_count_ = 0;
    std::uint64_t empty_frame_count_ = 0;
    std::uint64_t frames_missing_syn_report_count_ = 0;
    std::uint64_t frames_misplaced_syn_report_count_ = 0;
    std::uint64_t split_frames_count_ = 0;

    std::uint64_t first_sequence_ = 0;
    std::uint64_t last_sequence_ = 0;

    std::uint64_t first_timestamp_ns_ = 0;
    std::uint64_t last_timestamp_ns_ = 0;
    std::uint64_t minimum_timestamp_ns_ = 0;
    std::uint64_t maximum_timestamp_ns_ = 0;

    std::uint64_t sequence_discontinuity_count_ = 0;
    std::uint64_t nonforward_sequence_count_ = 0;
    std::uint64_t missing_sequence_count_ = 0;
    bool missing_sequence_count_overflowed_ = false;

    std::uint64_t timestamp_regression_count_ = 0;
    std::uint64_t same_timestamp_count_ = 0;

    std::uint64_t below_quarter_period_count_ = 0;
    std::uint64_t below_half_period_count_ = 0;
    std::uint64_t below_three_quarters_period_count_ = 0;

    std::uint64_t short_delta_context_count_ = 0;
    std::uint64_t short_delta_with_previous_count_ = 0;
    std::uint64_t short_delta_with_next_count_ = 0;
    std::uint64_t short_delta_with_both_neighbors_count_ = 0;

    std::uint64_t previous_plus_short_compensation_count_ = 0;
    std::uint64_t short_plus_next_compensation_count_ = 0;
    std::uint64_t either_pair_compensation_count_ = 0;
    std::uint64_t both_pair_compensation_count_ = 0;
    std::uint64_t triplet_compensation_count_ = 0;

    long double delta_sum_ns_ = 0.0L;
    std::vector<std::uint64_t> deltas_ns_;

    std::vector<std::uint64_t> histogram_counts_;
    std::uint64_t histogram_overflow_count_ = 0;

    std::optional<timestamp_pair_t> minimum_delta_;
    std::optional<timestamp_pair_t> minimum_positive_delta_;

    std::optional<delta_context_t> pending_delta_;
    std::vector<delta_context_t> shortest_delta_contexts_;

    std::optional<timestamp_regression_t> first_timestamp_regression_;
    std::optional<sequence_discontinuity_t> first_sequence_discontinuity_;
};

template <typename quantizer_t> class poll_interval_quantizer_stats_t
{
public:
    using timestamp_t = typename quantizer_t::timestamp_t;
    using period_t = typename quantizer_t::period_t;
    using residual_t = typename quantizer_t::residual_t;
    using tick_count_t = typename quantizer_t::tick_count_t;
    using interval_t = typename quantizer_t::interval_t;
    using status_t = typename quantizer_t::status_t;

    auto observe(timestamp_t timestamp, interval_t const& interval, residual_t residual) -> void
    {
        ++observations_;

        switch (interval.status)
        {
            case status_t::initialized:
                ++initializations_;
                forced_run_ = 0;
                break;

            case status_t::continuous:
                ++continuous_intervals_;
                observe_continuous_interval(timestamp, interval, residual);
                break;

            case status_t::timestamp_regressed:
                ++timestamp_regressions_;
                forced_run_ = 0;
                break;
        }

        if (!have_residual_)
        {
            minimum_residual_ = residual;
            maximum_residual_ = residual;
            have_residual_ = true;
        }
        else
        {
            if (residual < minimum_residual_) minimum_residual_ = residual;
            if (residual > maximum_residual_) maximum_residual_ = residual;
        }

        previous_timestamp_ = timestamp;
        previous_residual_ = residual;
        final_residual_ = residual;
        have_previous_ = true;
    }

    auto print() const -> void { print(std::cout); }

    auto print(std::ostream& out) const -> void
    {
        out << "poll interval quantizer\n";
        print_count(out, "observations", observations_);
        print_count(out, "initializations", initializations_);
        print_count(out, "continuous intervals", continuous_intervals_);
        print_count(out, "timestamp regressions", timestamp_regressions_);

        out << '\n' << "tick inference\n";
        print_count(out, "inferred elapsed ticks", inferred_elapsed_ticks_);
        print_count(out, "inferred hidden zero ticks", inferred_hidden_zero_ticks_);
        print_count_and_percentage(
            out, "minimum-tick-forced intervals", minimum_tick_forced_intervals_, continuous_intervals_);
        print_count(out, "longest forced run", longest_forced_run_);
        print_count(out, "maximum elapsed ticks", maximum_elapsed_ticks_);

        out << '\n' << "elapsed-tick distribution\n";
        print_histogram_row(out, "1", tick_histogram_[0]);
        print_histogram_row(out, "2", tick_histogram_[1]);
        print_histogram_row(out, "3", tick_histogram_[2]);
        print_histogram_row(out, "4", tick_histogram_[3]);
        print_histogram_row(out, "5-8", tick_histogram_[4]);
        print_histogram_row(out, "9-16", tick_histogram_[5]);
        print_histogram_row(out, "17-64", tick_histogram_[6]);
        print_histogram_row(out, "65+", tick_histogram_[7]);

        out << '\n' << "residual\n";

        if (!have_residual_) { out << "  no observations\n"; }
        else
        {
            out << "  minimum residual                    " << minimum_residual_ << " ns\n";
            out << "  maximum residual                    " << maximum_residual_ << " ns\n";
            out << "  final residual                      " << final_residual_ << " ns\n";
        }

        out << '\n' << "conservation\n";
        out << "  accumulated identity error          " << conservation_error_ << " ns\n";
    }

private:
    static constexpr auto histogram_index(tick_count_t ticks) noexcept -> std::size_t
    {
        if (ticks <= 4) return static_cast<std::size_t>(ticks - 1);
        if (ticks <= 8) return 4;
        if (ticks <= 16) return 5;
        if (ticks <= 64) return 6;
        return 7;
    }

    auto observe_continuous_interval(timestamp_t timestamp, interval_t const& interval, residual_t residual) -> void
    {
        assert(have_previous_);
        assert(timestamp >= previous_timestamp_);
        assert(interval.elapsed_ticks != 0);

        auto const raw_interval = timestamp - previous_timestamp_;

        using tick_count_fixed_t = fixed_t<tick_count_t, 0>;

        auto const quantized_interval = residual_t::convert(
            multiply(interval.report_period, tick_count_fixed_t::literal(interval.elapsed_ticks)));

        // Check the recurrence directly for every individual observation:
        //
        //     raw + previous residual
        //         = quantized + current residual
        conservation_error_ += residual_t::convert(raw_interval) + previous_residual_ - quantized_interval - residual;

        assert(inferred_elapsed_ticks_ <= max<uint64_t>() - interval.elapsed_ticks);
        inferred_elapsed_ticks_ += interval.elapsed_ticks;

        auto const hidden_zero_ticks = interval.hidden_zero_ticks();
        assert(inferred_hidden_zero_ticks_ <= max<uint64_t>() - hidden_zero_ticks);
        inferred_hidden_zero_ticks_ += hidden_zero_ticks;

        if (interval.minimum_tick_forced)
        {
            ++minimum_tick_forced_intervals_;
            ++forced_run_;

            if (forced_run_ > longest_forced_run_) longest_forced_run_ = forced_run_;
        }
        else
        {
            forced_run_ = 0;
        }

        if (interval.elapsed_ticks > maximum_elapsed_ticks_) maximum_elapsed_ticks_ = interval.elapsed_ticks;

        ++tick_histogram_[histogram_index(interval.elapsed_ticks)];
    }

    static auto percentage(uint64_t count, uint64_t total) noexcept -> double
    {
        if (total == 0) return 0.0;

        return static_cast<double>(count) * 100.0 / static_cast<double>(total);
    }

    static auto print_count(std::ostream& out, char const* label, uint64_t count) -> void
    {
        out << "  " << std::left << std::setw(38) << label << std::right << std::setw(12) << count << '\n';
    }

    static auto print_count_and_percentage(std::ostream& out, char const* label, uint64_t count, uint64_t total) -> void
    {
        out << "  " << std::left << std::setw(38) << label << std::right << std::setw(12) << count << "  " << std::fixed
            << std::setprecision(5) << std::setw(10) << percentage(count, total) << "%\n";
    }

    auto print_histogram_row(std::ostream& out, char const* label, uint64_t count) const -> void
    {
        out << "  " << std::left << std::setw(38) << label << std::right << std::setw(12) << count << "  " << std::fixed
            << std::setprecision(5) << std::setw(10) << percentage(count, continuous_intervals_) << "%\n";
    }

    uint64_t observations_{};
    uint64_t initializations_{};
    uint64_t continuous_intervals_{};
    uint64_t timestamp_regressions_{};

    uint64_t inferred_elapsed_ticks_{};
    uint64_t inferred_hidden_zero_ticks_{};
    uint64_t minimum_tick_forced_intervals_{};
    uint64_t forced_run_{};
    uint64_t longest_forced_run_{};

    tick_count_t maximum_elapsed_ticks_{};
    std::array<uint64_t, 8> tick_histogram_{};

    timestamp_t previous_timestamp_{};
    residual_t previous_residual_{};
    residual_t minimum_residual_{};
    residual_t maximum_residual_{};
    residual_t final_residual_{};
    residual_t conservation_error_{};

    bool have_previous_{};
    bool have_residual_{};
};

[[nodiscard]] auto run_stats(char const* path, long double expected_rate_hz) -> int
{
    auto opened = open_capture_file(path);

    if (!opened)
    {
        auto const& error = opened.error();

        std::fprintf(stderr, "%s: open failed", path);

        if (error.system_error != 0) { std::fprintf(stderr, ": %s", std::strerror(error.system_error)); }

        std::fputc('\n', stderr);
        return EXIT_FAILURE;
    }

    auto stream = std::move(*opened);
    auto timestamp_stats = timestamp_stats_t{expected_rate_hz};

    auto const period = to_fixed<poll_interval_quantizer_t::period_t>(1'000'000'000.0L / expected_rate_hz);
    auto poll_interval_quantizer = poll_interval_quantizer_t{};
    auto poll_interval_quantizer_stats = poll_interval_quantizer_stats_t<poll_interval_quantizer_t>{};
    auto complete = true;

    while (!stop_requested)
    {
        auto result = stream.read_input_values();

        if (!result)
        {
            if (result.error().code == capture_stream_error_code_t::interrupted)
            {
                if (stop_requested)
                {
                    complete = false;
                    break;
                }

                continue;
            }

            print_decode_error(path, result.error());
            return EXIT_FAILURE;
        }

        if (!result->has_value()) break;

        // The view expires on the next read, but observe() copies everything it retains.
        timestamp_stats.observe(result->value());

        auto const timestamp = typename poll_interval_quantizer_t::timestamp_t{result->value().timestamp_ns};
        auto const interval = poll_interval_quantizer.observe(timestamp, period);
        poll_interval_quantizer_stats.observe(timestamp, interval, poll_interval_quantizer.residual());
    }

    if (stop_requested)
    {
        complete = false;
        std::fprintf(stderr, "analysis interrupted; reported statistics are partial\n");
    }

    timestamp_stats.print();
    std::fprintf(stderr, "\n");
    poll_interval_quantizer_stats.print();

    return complete ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace
} // namespace crv

auto main(int argc, char** argv) -> int
{
    using namespace crv;

    if (argc < 3 || 3 < argc)
    {
        std::fprintf(stderr, "usage: %s CAPTURE_FILE EXPECTED_POLL_RATE_HZ\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (!install_signal_handlers())
    {
        std::fprintf(stderr, "failed to install signal handlers: %s\n", std::strerror(errno));
        return EXIT_FAILURE;
    }

    auto expected_rate_hz = parse_expected_rate(argv[2]);
    if (!expected_rate_hz || *expected_rate_hz < 0.0L)
    {
        std::fprintf(stderr, "invalid expected polling rate: %s\n", argv[2]);
        return EXIT_FAILURE;
    }

    return run_stats(argv[1], *expected_rate_hz);
}
