// SPDX-License-Identifier: MIT

/// \file
/// \brief reports timestamp statistics from a raw input-value capture
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/io/capture/file.hpp>
#include <crv/io/capture/stream.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cerrno>
#include <cmath>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <string>
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

    static constexpr auto visible_span_lags = std::array<std::size_t, 9>{1, 2, 4, 8, 16, 32, 64, 128, 256};
    static constexpr auto visible_span_probabilities = std::array<long double, 5>{0.0L, 0.001L, 0.01L, 0.05L, 0.10L};
    static constexpr auto visible_span_statistic_names
        = std::array<char const*, 5>{"minimum", "p0.1", "p1", "p5", "p10"};

    /*
        A percentile is reported only when the distribution contains at least
        approximately one sample in its lower tail.
    */
    static constexpr auto visible_span_minimum_sample_counts = std::array<std::size_t, 5>{1, 1'000, 100, 20, 10};

    static constexpr auto temporal_support_block_size = std::size_t{8'192};
    static constexpr auto temporal_support_lags = std::array<std::size_t, 4>{32, 64, 128, 256};

    static constexpr auto temporal_fit_quantile_indices = std::array<std::size_t, 2>{2, 4};
    static constexpr auto temporal_fit_branch_names = std::array<char const*, 2>{"p1", "p10"};

    static constexpr auto across_block_probabilities
        = std::array<long double, 7>{0.0L, 0.01L, 0.05L, 0.50L, 0.95L, 0.99L, 1.0L};
    static constexpr auto across_block_minimum_sample_counts = std::array<std::size_t, 7>{1, 100, 20, 1, 20, 100, 1};

    static constexpr auto temporal_period_error_thresholds = std::array<long double, 3>{0.0025L, 0.005L, 0.010L};

    static constexpr auto displayed_worst_block_count = std::size_t{5};

    struct delta_context_t
    {
        timestamp_pair_t sample;
        std::optional<std::uint64_t> previous_delta_ns;
        std::optional<std::uint64_t> next_delta_ns;
    };

    enum class visible_span_sampling_t
    {
        overlapping,
        disjoint,
    };

    struct visible_span_summary_t
    {
        std::size_t sample_count{};
        std::array<std::optional<long double>, visible_span_probabilities.size()> quantiles_ns{};
    };

    struct delta_chain_t
    {
        // Half-open positions in the complete observed-report stream.
        std::size_t report_begin;
        std::size_t report_end;

        // Half-open positions in deltas_ns_.
        std::size_t delta_begin;
        std::size_t delta_end;
    };

    struct temporal_block_timing_t
    {
        std::uint64_t first_timestamp_ns;
        std::uint64_t last_timestamp_ns;
        bool timestamp_regressed{};
    };

    struct common_intercept_fit_t
    {
        long double intercept_ns;
        long double endpoint_coefficient_ns;
        long double rmse_ns;
    };

    struct temporal_block_summary_t
    {
        std::size_t report_begin;
        std::size_t report_end;
        std::uint64_t first_timestamp_ns;
        std::uint64_t last_timestamp_ns;
        std::optional<std::uint64_t> wall_duration_ns;
        bool timestamp_regressed{};
        std::array<visible_span_summary_t, temporal_support_lags.size()> overlapping{};
        std::array<std::optional<common_intercept_fit_t>, temporal_fit_quantile_indices.size()> fits{};
    };

    struct temporal_support_summary_t
    {
        std::vector<temporal_block_summary_t> blocks;
        std::array<visible_span_summary_t, temporal_support_lags.size()> disjoint{};
    };

    struct across_block_distribution_t
    {
        std::size_t sample_count{};
        std::array<std::optional<long double>, across_block_probabilities.size()> quantiles{};
    };

    struct worst_case_category_t
    {
        char const* name;
        std::vector<std::size_t> block_indices;
    };

public:
    explicit timestamp_stats_t(std::optional<long double> expected_rate_hz,
        std::optional<std::string> temporal_support_sidecar_path = std::nullopt)
        : expected_rate_hz_{expected_rate_hz}, temporal_support_sidecar_path_{std::move(temporal_support_sidecar_path)},
          histogram_counts_(histogram_bin_count, 0)
    {
        if (expected_rate_hz_) { expected_period_ns_ = 1'000'000'000.0L / *expected_rate_hz_; }
    }

    auto observe(capture_input_values_view_t const& frame) -> void
    {
        ++frame_count_;

        assert(frame_count_ <= std::numeric_limits<std::size_t>::max());

        auto const current_report_index = static_cast<std::size_t>(frame_count_ - 1);

        observe_temporal_block_timestamp(current_report_index, frame.timestamp_ns);

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

            current_chain_report_begin_ = current_report_index;
            current_chain_delta_begin_ = deltas_ns_.size();

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

            break_delta_chain(current_report_index);
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

            break_delta_chain(current_report_index);
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

    auto print() -> bool
    {
        // The final delta has no known successor until input ends.
        finish_delta_chain();

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

            auto const empty_summary = temporal_support_summary_t{};
            return write_temporal_support_sidecar(empty_summary);
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
            auto const temporal_support_summary = summarize_temporal_support();
            auto const sidecar_written = write_temporal_support_sidecar(temporal_support_summary);

            std::fprintf(stdout, "\nno valid contiguous timestamp deltas were available\n");
            print_temporal_support(temporal_support_summary);
            print_integrity_examples();
            return sidecar_written;
        }

        auto const visible_span_summaries = summarize_visible_spans();
        auto const temporal_support_summary = summarize_temporal_support();
        auto const sidecar_written = write_temporal_support_sidecar(temporal_support_summary);

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

        print_visible_span_distributions(visible_span_summaries);
        print_temporal_support(temporal_support_summary);

        if (expected_period_ns_)
        {
            std::fprintf(stdout, "\nexpected polling rate\n");
            std::fprintf(stdout, "  %-37s %12.3Lf Hz\n", "configured rate", *expected_rate_hz_);

            print_duration("expected period", *expected_period_ns_);
            print_count("delta < 25% expected period", below_quarter_period_count_, deltas_ns_.size());
            print_count("delta < 50% expected period", below_half_period_count_, deltas_ns_.size());
            print_count("delta < 75% expected period", below_three_quarters_period_count_, deltas_ns_.size());

            print_short_delta_compensation();
        }
        else
        {
            std::fprintf(stdout,
                "\nno expected polling rate was supplied; "
                "rate-relative and compensation statistics were not calculated\n");
        }

        print_histogram_peaks();
        print_shortest_delta_contexts();
        print_integrity_examples();

        return sidecar_written;
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

        if (expected_period_ns_)
        {
            auto const delta = static_cast<long double>(sample.delta_ns);

            if (delta < *expected_period_ns_ * 0.25L) ++below_quarter_period_count_;
            if (delta < *expected_period_ns_ * 0.50L) ++below_half_period_count_;
            if (delta < *expected_period_ns_ * 0.75L) ++below_three_quarters_period_count_;
        }

        auto previous_delta_ns = std::optional<std::uint64_t>{};

        if (pending_delta_)
        {
            /*
                A pending context can only survive when this delta immediately
                follows it. Gaps and regressions call break_delta_chain().
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

    auto observe_temporal_block_timestamp(std::size_t report_index, std::uint64_t timestamp_ns) -> void
    {
        auto const block_index = report_index / temporal_support_block_size;

        if (block_index == temporal_block_timings_.size())
        {
            temporal_block_timings_.push_back(temporal_block_timing_t{
                .first_timestamp_ns = timestamp_ns,
                .last_timestamp_ns = timestamp_ns,
            });

            return;
        }

        assert(block_index + 1 == temporal_block_timings_.size());

        auto& timing = temporal_block_timings_.back();

        if (timestamp_ns < timing.last_timestamp_ns) timing.timestamp_regressed = true;

        timing.last_timestamp_ns = timestamp_ns;
    }

    [[nodiscard]] auto report_count() const noexcept -> std::size_t
    {
        assert(frame_count_ <= std::numeric_limits<std::size_t>::max());

        return static_cast<std::size_t>(frame_count_);
    }

    auto close_delta_chain(std::size_t report_end) -> void
    {
        assert(initialized_);
        assert(current_chain_report_begin_ < report_end);
        assert(report_end <= report_count());
        assert(current_chain_delta_begin_ <= deltas_ns_.size());

        if (!delta_chains_.empty())
        {
            assert(delta_chains_.back().report_end == current_chain_report_begin_);
            assert(delta_chains_.back().delta_end == current_chain_delta_begin_);
        }

        auto const delta_end = deltas_ns_.size();
        auto const chain_report_count = report_end - current_chain_report_begin_;
        auto const chain_delta_count = delta_end - current_chain_delta_begin_;

        /*
            A valid timestamp chain with n reports contains exactly n - 1
            contiguous deltas. A one-report chain therefore contains no deltas.
        */
        assert(chain_report_count == chain_delta_count + 1);

        delta_chains_.push_back(delta_chain_t{
            .report_begin = current_chain_report_begin_,
            .report_end = report_end,
            .delta_begin = current_chain_delta_begin_,
            .delta_end = delta_end,
        });
    }

    auto break_delta_chain(std::size_t next_chain_report_begin) -> void
    {
        assert(initialized_);
        assert(current_chain_report_begin_ < next_chain_report_begin);

        if (pending_delta_) finalize_pending_delta();

        close_delta_chain(next_chain_report_begin);

        current_chain_report_begin_ = next_chain_report_begin;
        current_chain_delta_begin_ = deltas_ns_.size();
    }

    auto finish_delta_chain() -> void
    {
        if (pending_delta_) finalize_pending_delta();

        if (!initialized_) return;

        close_delta_chain(report_count());
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
        if (!expected_period_ns_) return;

        auto const delta = static_cast<long double>(context.sample.delta_ns);
        auto const short_threshold = *expected_period_ns_ * 0.50L;

        if (delta >= short_threshold) return;

        ++short_delta_context_count_;

        auto previous_compensates = false;
        auto next_compensates = false;

        if (context.previous_delta_ns)
        {
            ++short_delta_with_previous_count_;

            auto const pair = static_cast<long double>(*context.previous_delta_ns) + delta;

            previous_compensates = approximately(pair, *expected_period_ns_ * 2.0L);

            if (previous_compensates) { ++previous_plus_short_compensation_count_; }
        }

        if (context.next_delta_ns)
        {
            ++short_delta_with_next_count_;

            auto const pair = delta + static_cast<long double>(*context.next_delta_ns);

            next_compensates = approximately(pair, *expected_period_ns_ * 2.0L);

            if (next_compensates) { ++short_plus_next_compensation_count_; }
        }

        if (previous_compensates || next_compensates) { ++either_pair_compensation_count_; }

        if (previous_compensates && next_compensates) { ++both_pair_compensation_count_; }

        if (context.previous_delta_ns && context.next_delta_ns)
        {
            ++short_delta_with_both_neighbors_count_;

            auto const triplet = static_cast<long double>(*context.previous_delta_ns) + delta
                + static_cast<long double>(*context.next_delta_ns);

            if (approximately(triplet, *expected_period_ns_ * 3.0L)) { ++triplet_compensation_count_; }
        }
    }

    [[nodiscard]] auto approximately(long double value, long double target) const noexcept -> bool
    {
        assert(expected_period_ns_);

        auto const tolerance = *expected_period_ns_ * compensation_tolerance_fraction;

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
        assert(expected_period_ns_);

        auto const short_threshold = *expected_period_ns_ * 0.50L;
        auto const pair_target = *expected_period_ns_ * 2.0L;
        auto const triplet_target = *expected_period_ns_ * 3.0L;
        auto const tolerance = *expected_period_ns_ * compensation_tolerance_fraction;

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

    [[nodiscard]] auto sum_delta_span(std::size_t delta_begin, std::size_t delta_count) const -> std::uint64_t
    {
        assert(delta_begin <= deltas_ns_.size());
        assert(delta_count <= deltas_ns_.size() - delta_begin);

        auto total_ns = std::uint64_t{0};

        for (auto offset = std::size_t{0}; offset < delta_count; ++offset)
        {
            auto const delta_ns = deltas_ns_[delta_begin + offset];

            assert(delta_ns <= std::numeric_limits<std::uint64_t>::max() - total_ns);

            total_ns += delta_ns;
        }

        return total_ns;
    }

    auto append_visible_span_totals(std::vector<std::uint64_t>& destination, delta_chain_t const& chain,
        std::size_t segment_report_begin, std::size_t segment_report_end, std::size_t lag,
        visible_span_sampling_t sampling) const -> void
    {
        assert(lag != 0);
        assert(chain.report_begin <= segment_report_begin);
        assert(segment_report_begin <= segment_report_end);
        assert(segment_report_end <= chain.report_end);

        auto const segment_report_count = segment_report_end - segment_report_begin;

        // A lag-m span needs m + 1 report timestamps.
        if (segment_report_count <= lag) return;

        auto const first_delta = chain.delta_begin + (segment_report_begin - chain.report_begin);
        auto const segment_delta_count = segment_report_count - 1;

        assert(first_delta <= chain.delta_end);
        assert(segment_delta_count <= chain.delta_end - first_delta);

        switch (sampling)
        {
            case visible_span_sampling_t::overlapping:
            {
                auto span_total_ns = sum_delta_span(first_delta, lag);

                destination.push_back(span_total_ns);

                auto const span_count = segment_report_count - lag;

                for (auto span_offset = std::size_t{1}; span_offset < span_count; ++span_offset)
                {
                    auto const departing_delta = deltas_ns_[first_delta + span_offset - 1];
                    auto const arriving_delta = deltas_ns_[first_delta + span_offset + lag - 1];

                    assert(departing_delta <= span_total_ns);
                    span_total_ns -= departing_delta;

                    assert(arriving_delta <= std::numeric_limits<std::uint64_t>::max() - span_total_ns);
                    span_total_ns += arriving_delta;

                    destination.push_back(span_total_ns);
                }

                break;
            }

            case visible_span_sampling_t::disjoint:
            {
                auto const one_past_final_start_offset = segment_report_count - lag;

                for (auto span_offset = std::size_t{0}; span_offset < one_past_final_start_offset; span_offset += lag)
                {
                    destination.push_back(sum_delta_span(first_delta + span_offset, lag));
                }

                break;
            }
        }
    }

    auto append_block_visible_span_totals(std::vector<std::uint64_t>& destination, std::size_t block_report_begin,
        std::size_t block_report_end, std::size_t lag, visible_span_sampling_t sampling) const -> void
    {
        assert(block_report_begin <= block_report_end);
        assert(block_report_end <= report_count());

        for (auto const& chain : delta_chains_)
        {
            if (chain.report_end <= block_report_begin) continue;
            if (chain.report_begin >= block_report_end) break;

            auto const segment_report_begin = std::max(block_report_begin, chain.report_begin);
            auto const segment_report_end = std::min(block_report_end, chain.report_end);

            assert(segment_report_begin < segment_report_end);

            append_visible_span_totals(destination, chain, segment_report_begin, segment_report_end, lag, sampling);
        }
    }

    [[nodiscard]] static auto summarize_visible_span_distribution(
        std::vector<std::uint64_t> span_totals_ns, std::size_t lag) -> visible_span_summary_t
    {
        assert(lag != 0);

        auto summary = visible_span_summary_t{
            .sample_count = span_totals_ns.size(),
        };

        if (span_totals_ns.empty()) return summary;

        std::sort(span_totals_ns.begin(), span_totals_ns.end());

        for (auto probability_index = std::size_t{0}; probability_index < visible_span_probabilities.size();
            ++probability_index)
        {
            if (span_totals_ns.size() < visible_span_minimum_sample_counts[probability_index]) continue;

            summary.quantiles_ns[probability_index]
                = quantile(span_totals_ns, visible_span_probabilities[probability_index])
                / static_cast<long double>(lag);
        }

        return summary;
    }

    [[nodiscard]] auto summarize_visible_spans() const -> std::array<visible_span_summary_t, visible_span_lags.size()>
    {
        auto summaries = std::array<visible_span_summary_t, visible_span_lags.size()>{};

        for (auto lag_index = std::size_t{0}; lag_index < visible_span_lags.size(); ++lag_index)
        {
            auto const lag = visible_span_lags[lag_index];

            auto span_totals_ns = std::vector<std::uint64_t>{};
            span_totals_ns.reserve(deltas_ns_.size());

            for (auto const& chain : delta_chains_)
            {
                append_visible_span_totals(span_totals_ns, chain, chain.report_begin, chain.report_end, lag,
                    visible_span_sampling_t::overlapping);
            }

            summaries[lag_index] = summarize_visible_span_distribution(std::move(span_totals_ns), lag);
        }

        // Every usable delta is exactly one lag-1 visible span.
        assert(summaries.front().sample_count == deltas_ns_.size());

        return summaries;
    }

    [[nodiscard]] static auto fit_common_intercept(temporal_block_summary_t const& block, std::size_t quantile_index)
        -> std::optional<common_intercept_fit_t>
    {
        auto x = std::array<long double, temporal_support_lags.size()>{};
        auto y = std::array<long double, temporal_support_lags.size()>{};

        for (auto lag_index = std::size_t{0}; lag_index < temporal_support_lags.size(); ++lag_index)
        {
            auto const value = block.overlapping[lag_index].quantiles_ns[quantile_index];

            if (!value) return std::nullopt;

            x[lag_index] = 1.0L / static_cast<long double>(temporal_support_lags[lag_index]);
            y[lag_index] = *value;
        }

        auto x_mean = 0.0L;
        auto y_mean = 0.0L;

        for (auto index = std::size_t{0}; index < x.size(); ++index)
        {
            x_mean += x[index];
            y_mean += y[index];
        }

        x_mean /= static_cast<long double>(x.size());
        y_mean /= static_cast<long double>(y.size());

        auto x_variance_sum = 0.0L;
        auto covariance_sum = 0.0L;

        for (auto index = std::size_t{0}; index < x.size(); ++index)
        {
            auto const centered_x = x[index] - x_mean;

            x_variance_sum += centered_x * centered_x;
            covariance_sum += centered_x * (y[index] - y_mean);
        }

        if (x_variance_sum == 0.0L) return std::nullopt;

        auto const endpoint_coefficient_ns = covariance_sum / x_variance_sum;
        auto const intercept_ns = y_mean - endpoint_coefficient_ns * x_mean;

        if (!std::isfinite(intercept_ns) || !std::isfinite(endpoint_coefficient_ns)) return std::nullopt;

        auto squared_error_sum = 0.0L;

        for (auto index = std::size_t{0}; index < x.size(); ++index)
        {
            auto const fitted = intercept_ns + endpoint_coefficient_ns * x[index];
            auto const residual = y[index] - fitted;

            squared_error_sum += residual * residual;
        }

        auto const rmse_ns = std::sqrt(squared_error_sum / static_cast<long double>(x.size()));

        if (!std::isfinite(rmse_ns)) return std::nullopt;

        return common_intercept_fit_t{
            .intercept_ns = intercept_ns,
            .endpoint_coefficient_ns = endpoint_coefficient_ns,
            .rmse_ns = rmse_ns,
        };
    }

    [[nodiscard]] auto summarize_temporal_support() const -> temporal_support_summary_t
    {
        auto result = temporal_support_summary_t{};

        auto disjoint_span_totals = std::array<std::vector<std::uint64_t>, temporal_support_lags.size()>{};

        for (auto lag_index = std::size_t{0}; lag_index < temporal_support_lags.size(); ++lag_index)
        {
            auto const lag = temporal_support_lags[lag_index];

            disjoint_span_totals[lag_index].reserve(deltas_ns_.size() / lag);
        }

        auto block_report_begin = std::size_t{0};

        while (block_report_begin < report_count())
        {
            auto const remaining_reports = report_count() - block_report_begin;
            auto const block_report_count = std::min(temporal_support_block_size, remaining_reports);
            auto const block_report_end = block_report_begin + block_report_count;
            auto const block_index = result.blocks.size();

            assert(block_index < temporal_block_timings_.size());

            auto const& timing = temporal_block_timings_[block_index];
            auto wall_duration_ns = std::optional<std::uint64_t>{};

            if (!timing.timestamp_regressed && timing.first_timestamp_ns <= timing.last_timestamp_ns)
            {
                wall_duration_ns = timing.last_timestamp_ns - timing.first_timestamp_ns;
            }

            auto block = temporal_block_summary_t{
                .report_begin = block_report_begin,
                .report_end = block_report_end,
                .first_timestamp_ns = timing.first_timestamp_ns,
                .last_timestamp_ns = timing.last_timestamp_ns,
                .wall_duration_ns = wall_duration_ns,
                .timestamp_regressed = timing.timestamp_regressed,
            };

            for (auto lag_index = std::size_t{0}; lag_index < temporal_support_lags.size(); ++lag_index)
            {
                auto const lag = temporal_support_lags[lag_index];

                auto overlapping_span_totals_ns = std::vector<std::uint64_t>{};
                overlapping_span_totals_ns.reserve(
                    block_report_count > lag ? block_report_count - lag : std::size_t{0});

                append_block_visible_span_totals(overlapping_span_totals_ns, block_report_begin, block_report_end, lag,
                    visible_span_sampling_t::overlapping);

                block.overlapping[lag_index]
                    = summarize_visible_span_distribution(std::move(overlapping_span_totals_ns), lag);

                /*
                    Disjoint windows restart at every fixed block boundary and
                    every valid timestamp-chain boundary. No span can cross
                    either kind of boundary.
                */
                append_block_visible_span_totals(disjoint_span_totals[lag_index], block_report_begin, block_report_end,
                    lag, visible_span_sampling_t::disjoint);
            }

            for (auto branch_index = std::size_t{0}; branch_index < temporal_fit_quantile_indices.size();
                ++branch_index)
            {
                block.fits[branch_index] = fit_common_intercept(block, temporal_fit_quantile_indices[branch_index]);
            }

            result.blocks.push_back(std::move(block));
            block_report_begin = block_report_end;
        }

        assert(result.blocks.size() == temporal_block_timings_.size());

        for (auto lag_index = std::size_t{0}; lag_index < temporal_support_lags.size(); ++lag_index)
        {
            result.disjoint[lag_index] = summarize_visible_span_distribution(
                std::move(disjoint_span_totals[lag_index]), temporal_support_lags[lag_index]);
        }

        return result;
    }

    [[nodiscard]] static auto quantile_long_double(std::vector<long double> const& sorted, long double probability)
        -> long double
    {
        assert(!sorted.empty());
        assert(probability >= 0.0L);
        assert(probability <= 1.0L);

        auto const position = probability * static_cast<long double>(sorted.size() - 1);
        auto const lower_index = static_cast<std::size_t>(position);
        auto const upper_index = std::min(lower_index + 1, sorted.size() - 1);
        auto const fraction = position - static_cast<long double>(lower_index);

        return sorted[lower_index] + fraction * (sorted[upper_index] - sorted[lower_index]);
    }

    [[nodiscard]] static auto summarize_across_blocks(std::vector<long double> values) -> across_block_distribution_t
    {
        auto summary = across_block_distribution_t{
            .sample_count = values.size(),
        };

        if (values.empty()) return summary;

        std::sort(values.begin(), values.end());

        for (auto index = std::size_t{0}; index < across_block_probabilities.size(); ++index)
        {
            if (values.size() < across_block_minimum_sample_counts[index]) continue;

            summary.quantiles[index] = quantile_long_double(values, across_block_probabilities[index]);
        }

        return summary;
    }

    static auto print_visible_span_table_header() -> void
    {
        std::fprintf(
            stdout, "    %5s %12s %12s %12s %12s %12s %12s\n", "lag", "samples", "minimum", "p0.1", "p1", "p5", "p10");
    }

    static auto print_visible_span_value(std::optional<long double> value_ns, std::size_t sample_count) -> void
    {
        if (value_ns)
        {
            std::fprintf(stdout, " %12.3Lf", *value_ns / 1'000.0L);
            return;
        }

        std::fprintf(stdout, " %12s", sample_count == 0 ? "none" : "insufficient");
    }

    static auto print_visible_span_summary_row(std::size_t lag, visible_span_summary_t const& summary) -> void
    {
        std::fprintf(stdout, "    %5zu %12zu", lag, summary.sample_count);

        for (auto const value_ns : summary.quantiles_ns) { print_visible_span_value(value_ns, summary.sample_count); }

        std::fputc('\n', stdout);
    }

    auto print_visible_span_distributions(
        std::array<visible_span_summary_t, visible_span_lags.size()> const& summaries) const -> void
    {
        std::fprintf(stdout, "\nnormalized visible-span distributions (us per report)\n");

        print_visible_span_table_header();

        for (auto index = std::size_t{0}; index < visible_span_lags.size(); ++index)
        {
            print_visible_span_summary_row(visible_span_lags[index], summaries[index]);
        }
    }

    static auto print_across_block_value(std::optional<long double> value, long double divisor) -> void
    {
        if (value)
        {
            std::fprintf(stdout, " %12.3Lf", *value / divisor);
            return;
        }

        std::fprintf(stdout, " %12s", "insufficient");
    }

    static auto print_across_block_distribution(across_block_distribution_t const& distribution, long double divisor)
        -> void
    {
        for (auto const value : distribution.quantiles) { print_across_block_value(value, divisor); }
    }

    auto print_temporal_statistic_stability(temporal_support_summary_t const& summary) const -> void
    {
        std::fprintf(stdout, "\nwithin-block statistic stability across time (us per report)\n");
        std::fprintf(stdout, "  %5s %-9s %8s %12s %12s %12s %12s %12s %12s %12s\n", "lag", "statistic", "blocks",
            "minimum", "p1", "p5", "median", "p95", "p99", "maximum");

        for (auto lag_index = std::size_t{0}; lag_index < temporal_support_lags.size(); ++lag_index)
        {
            for (auto statistic_index = std::size_t{0}; statistic_index < visible_span_probabilities.size();
                ++statistic_index)
            {
                auto values = std::vector<long double>{};
                values.reserve(summary.blocks.size());

                for (auto const& block : summary.blocks)
                {
                    auto const value = block.overlapping[lag_index].quantiles_ns[statistic_index];
                    if (value) values.push_back(*value);
                }

                auto const distribution = summarize_across_blocks(std::move(values));

                std::fprintf(stdout, "  %5zu %-9s %8zu", temporal_support_lags[lag_index],
                    visible_span_statistic_names[statistic_index], distribution.sample_count);
                print_across_block_distribution(distribution, 1'000.0L);
                std::fputc('\n', stdout);
            }
        }
    }

    auto print_temporal_period_thresholds(temporal_support_summary_t const& summary) const -> void
    {
        if (!expected_period_ns_)
        {
            std::fprintf(stdout,
                "\nno expected polling rate was supplied; temporal period-error threshold counts are unavailable\n");
            return;
        }

        std::fprintf(stdout, "\nblocks near the configured expected period\n");
        std::fprintf(stdout, "  validation thresholds only; they are not estimator assumptions\n");
        std::fprintf(stdout, "  %5s %-9s %8s %14s %14s %14s\n", "lag", "statistic", "blocks", "within 0.25%",
            "within 0.5%", "within 1.0%");

        for (auto lag_index = std::size_t{0}; lag_index < temporal_support_lags.size(); ++lag_index)
        {
            for (auto const statistic_index : temporal_fit_quantile_indices)
            {
                auto eligible = std::size_t{0};
                auto counts = std::array<std::size_t, temporal_period_error_thresholds.size()>{};

                for (auto const& block : summary.blocks)
                {
                    auto const value = block.overlapping[lag_index].quantiles_ns[statistic_index];
                    if (!value) continue;

                    ++eligible;

                    auto const relative_error = std::fabs(*value - *expected_period_ns_) / *expected_period_ns_;

                    for (auto threshold_index = std::size_t{0};
                        threshold_index < temporal_period_error_thresholds.size(); ++threshold_index)
                    {
                        if (relative_error <= temporal_period_error_thresholds[threshold_index])
                        {
                            ++counts[threshold_index];
                        }
                    }
                }

                std::fprintf(stdout, "  %5zu %-9s %8zu %7zu/%-6zu %7zu/%-6zu %7zu/%-6zu\n",
                    temporal_support_lags[lag_index], visible_span_statistic_names[statistic_index], eligible,
                    counts[0], eligible, counts[1], eligible, counts[2], eligible);
            }
        }
    }

    auto print_common_intercept_stability(temporal_support_summary_t const& summary) const -> void
    {
        std::fprintf(stdout, "\ncommon-intercept fits q_m = T + C/m\n");
        std::fprintf(stdout, "  fits require all four lags: 32, 64, 128, and 256\n");

        for (auto branch_index = std::size_t{0}; branch_index < temporal_fit_branch_names.size(); ++branch_index)
        {
            auto intercepts = std::vector<long double>{};
            auto rmses = std::vector<long double>{};
            intercepts.reserve(summary.blocks.size());
            rmses.reserve(summary.blocks.size());

            for (auto const& block : summary.blocks)
            {
                auto const& fit = block.fits[branch_index];
                if (!fit) continue;

                intercepts.push_back(fit->intercept_ns);
                rmses.push_back(fit->rmse_ns);
            }

            auto const fitted_blocks = intercepts.size();
            auto const failed_blocks = summary.blocks.size() - fitted_blocks;
            auto const intercept_distribution = summarize_across_blocks(std::move(intercepts));
            auto const rmse_distribution = summarize_across_blocks(std::move(rmses));

            std::fprintf(stdout, "\n  branch %-4s  fitted blocks %zu  failed blocks %zu\n",
                temporal_fit_branch_names[branch_index], fitted_blocks, failed_blocks);

            std::fprintf(stdout, "    %-9s %12s %12s %12s %12s %12s %12s %12s\n", "", "minimum", "p1", "p5", "median",
                "p95", "p99", "maximum");
            std::fprintf(stdout, "    %-9s", "T (us)");
            print_across_block_distribution(intercept_distribution, 1'000.0L);
            std::fputc('\n', stdout);

            std::fprintf(stdout, "    fit RMSE (us)  median ");
            print_across_block_value(rmse_distribution.quantiles[3], 1'000.0L);
            std::fprintf(stdout, "  p95 ");
            print_across_block_value(rmse_distribution.quantiles[4], 1'000.0L);
            std::fprintf(stdout, "  maximum ");
            print_across_block_value(rmse_distribution.quantiles[6], 1'000.0L);
            std::fputc('\n', stdout);
        }
    }

    auto print_block_duration_stability(temporal_support_summary_t const& summary) const -> void
    {
        auto full_block_durations = std::vector<long double>{};
        full_block_durations.reserve(summary.blocks.size());

        auto full_block_count = std::size_t{0};
        auto unavailable_full_block_count = std::size_t{0};
        auto partial_block_indices = std::vector<std::size_t>{};

        for (auto block_index = std::size_t{0}; block_index < summary.blocks.size(); ++block_index)
        {
            auto const& block = summary.blocks[block_index];

            assert(block.report_begin <= block.report_end);

            auto const block_report_count = block.report_end - block.report_begin;

            if (block_report_count == temporal_support_block_size)
            {
                ++full_block_count;

                if (block.wall_duration_ns)
                {
                    full_block_durations.push_back(static_cast<long double>(*block.wall_duration_ns));
                }
                else
                {
                    ++unavailable_full_block_count;
                }

                continue;
            }

            /*
                Fixed report blocks can produce at most one partial block, and
                it can only be the final block in the capture.
            */
            assert(block_report_count < temporal_support_block_size);
            assert(block_index + 1 == summary.blocks.size());

            partial_block_indices.push_back(block_index);
        }

        auto const distribution = summarize_across_blocks(std::move(full_block_durations));

        std::fprintf(stdout, "\nobserved wall-time per full fixed report block (seconds)\n");
        std::fprintf(stdout, "  block size %zu reports  full blocks %zu  available %zu  unavailable %zu\n",
            temporal_support_block_size, full_block_count, distribution.sample_count, unavailable_full_block_count);

        if (distribution.sample_count == 0)
        {
            std::fprintf(stdout, "  no full blocks had an available wall-time duration\n");
        }
        else
        {
            std::fprintf(stdout, "  %12s %12s %12s %12s %12s %12s %12s\n", "minimum", "p1", "p5", "median", "p95",
                "p99", "maximum");
            std::fprintf(stdout, " ");
            print_across_block_distribution(distribution, 1'000'000'000.0L);
            std::fputc('\n', stdout);
        }

        if (partial_block_indices.empty())
        {
            std::fprintf(stdout, "  trailing partial block: none\n");
            return;
        }

        for (auto const block_index : partial_block_indices)
        {
            auto const& block = summary.blocks[block_index];
            auto const block_report_count = block.report_end - block.report_begin;

            std::fprintf(stdout, "  trailing partial block %zu: reports [%zu, %zu) (%zu reports), ", block_index,
                block.report_begin, block.report_end, block_report_count);

            if (block.wall_duration_ns)
            {
                std::fprintf(stdout, "duration %.6Lf s; excluded from full-block distribution\n",
                    static_cast<long double>(*block.wall_duration_ns) / 1'000'000'000.0L);
            }
            else
            {
                std::fprintf(stdout,
                    "duration unavailable (timestamp regression within block); "
                    "excluded from full-block distribution\n");
            }
        }
    }

    [[nodiscard]] static auto select_metric_extremes(
        std::vector<std::pair<long double, std::size_t>> metrics, bool largest) -> std::vector<std::size_t>
    {
        std::sort(metrics.begin(), metrics.end(), [largest](auto const& left, auto const& right) {
            if (left.first != right.first) return largest ? left.first > right.first : left.first < right.first;
            return left.second < right.second;
        });

        if (metrics.size() > displayed_worst_block_count) metrics.resize(displayed_worst_block_count);

        auto result = std::vector<std::size_t>{};
        result.reserve(metrics.size());

        for (auto const& metric : metrics) result.push_back(metric.second);

        return result;
    }

    [[nodiscard]] auto build_worst_case_categories(temporal_support_summary_t const& summary) const
        -> std::vector<worst_case_category_t>
    {
        auto categories = std::vector<worst_case_category_t>{};

        if (expected_period_ns_)
        {
            auto metrics = std::vector<std::pair<long double, std::size_t>>{};

            for (auto block_index = std::size_t{0}; block_index < summary.blocks.size(); ++block_index)
            {
                auto greatest_error = std::optional<long double>{};

                for (auto const& fit : summary.blocks[block_index].fits)
                {
                    if (!fit) continue;

                    auto const error = std::fabs(fit->intercept_ns - *expected_period_ns_);
                    if (!greatest_error || error > *greatest_error) greatest_error = error;
                }

                if (greatest_error) metrics.emplace_back(*greatest_error, block_index);
            }

            categories.push_back(worst_case_category_t{
                .name = "largest absolute fitted-period error",
                .block_indices = select_metric_extremes(std::move(metrics), true),
            });
        }

        {
            auto metrics = std::vector<std::pair<long double, std::size_t>>{};

            for (auto block_index = std::size_t{0}; block_index < summary.blocks.size(); ++block_index)
            {
                auto greatest_rmse = std::optional<long double>{};

                for (auto const& fit : summary.blocks[block_index].fits)
                {
                    if (!fit) continue;
                    if (!greatest_rmse || fit->rmse_ns > *greatest_rmse) greatest_rmse = fit->rmse_ns;
                }

                if (greatest_rmse) metrics.emplace_back(*greatest_rmse, block_index);
            }

            categories.push_back(worst_case_category_t{
                .name = "largest fit RMSE",
                .block_indices = select_metric_extremes(std::move(metrics), true),
            });
        }

        auto lag_256_index = temporal_support_lags.size() - 1;
        auto p1_index = temporal_fit_quantile_indices.front();
        auto lag_256_p1 = std::vector<std::pair<long double, std::size_t>>{};

        for (auto block_index = std::size_t{0}; block_index < summary.blocks.size(); ++block_index)
        {
            auto const value = summary.blocks[block_index].overlapping[lag_256_index].quantiles_ns[p1_index];
            if (value) lag_256_p1.emplace_back(*value, block_index);
        }

        categories.push_back(worst_case_category_t{
            .name = "lowest lag-256 p1",
            .block_indices = select_metric_extremes(lag_256_p1, false),
        });
        categories.push_back(worst_case_category_t{
            .name = "highest lag-256 p1",
            .block_indices = select_metric_extremes(std::move(lag_256_p1), true),
        });

        auto failed = std::vector<std::size_t>{};

        for (auto block_index = std::size_t{0}; block_index < summary.blocks.size(); ++block_index)
        {
            auto const& fits = summary.blocks[block_index].fits;
            if (std::any_of(fits.begin(), fits.end(), [](auto const& fit) { return !fit; }))
            {
                failed.push_back(block_index);
                if (failed.size() == displayed_worst_block_count) break;
            }
        }

        categories.push_back(worst_case_category_t{
            .name = "failed common-intercept fit",
            .block_indices = std::move(failed),
        });

        return categories;
    }

    static auto category_contains(worst_case_category_t const& category, std::size_t block_index) -> bool
    {
        return std::find(category.block_indices.begin(), category.block_indices.end(), block_index)
            != category.block_indices.end();
    }

    auto print_worst_case_blocks(temporal_support_summary_t const& summary) const -> void
    {
        auto const categories = build_worst_case_categories(summary);
        auto selected = std::vector<std::size_t>{};

        for (auto const& category : categories)
        {
            for (auto const block_index : category.block_indices)
            {
                if (std::find(selected.begin(), selected.end(), block_index) == selected.end())
                {
                    selected.push_back(block_index);
                }
            }
        }

        std::sort(selected.begin(), selected.end());

        std::fprintf(stdout, "\nselected worst-case temporal blocks\n");

        if (selected.empty())
        {
            std::fprintf(stdout, "  none\n");
            return;
        }

        for (auto const block_index : selected)
        {
            auto const& block = summary.blocks[block_index];

            std::fprintf(stdout, "\n  block %zu: observed reports [%zu, %zu) (%zu reports)\n", block_index,
                block.report_begin, block.report_end, block.report_end - block.report_begin);
            std::fprintf(stdout, "    selected by: ");

            auto first_reason = true;
            for (auto const& category : categories)
            {
                if (!category_contains(category, block_index)) continue;

                std::fprintf(stdout, "%s%s", first_reason ? "" : "; ", category.name);
                first_reason = false;
            }
            std::fputc('\n', stdout);

            std::fprintf(stdout, "    capture-clock range: %llu -> %llu ns",
                static_cast<unsigned long long>(block.first_timestamp_ns),
                static_cast<unsigned long long>(block.last_timestamp_ns));

            if (block.wall_duration_ns)
            {
                std::fprintf(stdout, "  duration %.6Lf s\n",
                    static_cast<long double>(*block.wall_duration_ns) / 1'000'000'000.0L);
            }
            else
            {
                std::fprintf(stdout, "  duration unavailable (timestamp regression within block)\n");
            }

            for (auto branch_index = std::size_t{0}; branch_index < temporal_fit_branch_names.size(); ++branch_index)
            {
                auto const& fit = block.fits[branch_index];
                if (!fit)
                {
                    std::fprintf(stdout, "    %s fit: failed\n", temporal_fit_branch_names[branch_index]);
                    continue;
                }

                std::fprintf(stdout, "    %s fit: T %.6Lf us  C %.6Lf us-reports  RMSE %.6Lf us\n",
                    temporal_fit_branch_names[branch_index], fit->intercept_ns / 1'000.0L,
                    fit->endpoint_coefficient_ns / 1'000.0L, fit->rmse_ns / 1'000.0L);
            }

            print_visible_span_table_header();

            for (auto lag_index = std::size_t{0}; lag_index < temporal_support_lags.size(); ++lag_index)
            {
                print_visible_span_summary_row(temporal_support_lags[lag_index], block.overlapping[lag_index]);
            }
        }
    }

    static auto write_sidecar_quantile(std::ostream& out, std::optional<long double> value, std::size_t sample_count)
        -> void
    {
        if (value) out << *value;
        else out << (sample_count == 0 ? "none" : "insufficient");
    }

    auto write_temporal_support_sidecar(temporal_support_summary_t const& summary) const -> bool
    {
        if (!temporal_support_sidecar_path_)
        {
            std::fprintf(stderr,
                "warning: no temporal-support TSV sidecar path was supplied; detailed per-block data was not written\n");
            return true;
        }

        errno = 0;
        auto out = std::ofstream{*temporal_support_sidecar_path_, std::ios::out | std::ios::trunc};

        if (!out)
        {
            std::fprintf(
                stderr, "%s: failed to open temporal-support TSV sidecar", temporal_support_sidecar_path_->c_str());
            if (errno != 0) std::fprintf(stderr, ": %s", std::strerror(errno));
            std::fputc('\n', stderr);
            return false;
        }

        out << std::setprecision(std::numeric_limits<long double>::max_digits10);
        out << "block\treport_begin\treport_end\treport_count"
               "\tfirst_timestamp_ns\tlast_timestamp_ns\twall_duration_ns\twall_duration_status"
               "\tlag\tsample_count\tminimum_ns\tp0_1_ns\tp1_ns\tp5_ns\tp10_ns"
               "\tp1_fit_status\tp1_T_ns\tp1_C_ns_reports\tp1_rmse_ns"
               "\tp10_fit_status\tp10_T_ns\tp10_C_ns_reports\tp10_rmse_ns\n";

        for (auto block_index = std::size_t{0}; block_index < summary.blocks.size(); ++block_index)
        {
            auto const& block = summary.blocks[block_index];

            for (auto lag_index = std::size_t{0}; lag_index < temporal_support_lags.size(); ++lag_index)
            {
                auto const& span = block.overlapping[lag_index];

                out << block_index << '\t' << block.report_begin << '\t' << block.report_end << '\t'
                    << block.report_end - block.report_begin << '\t' << block.first_timestamp_ns << '\t'
                    << block.last_timestamp_ns << '\t';

                if (block.wall_duration_ns) out << *block.wall_duration_ns << "\tok";
                else out << "unavailable\ttimestamp_regressed";

                out << '\t' << temporal_support_lags[lag_index] << '\t' << span.sample_count;

                for (auto const quantile : span.quantiles_ns)
                {
                    out << '\t';
                    write_sidecar_quantile(out, quantile, span.sample_count);
                }

                for (auto branch_index = std::size_t{0}; branch_index < block.fits.size(); ++branch_index)
                {
                    auto const& fit = block.fits[branch_index];

                    if (fit)
                    {
                        out << "\tok\t" << fit->intercept_ns << '\t' << fit->endpoint_coefficient_ns << '\t'
                            << fit->rmse_ns;
                    }
                    else
                    {
                        out << "\tfailed\t\t\t";
                    }
                }

                out << '\n';
            }
        }

        out.flush();
        out.close();

        if (!out)
        {
            std::fprintf(stderr, "%s: failed while writing or closing temporal-support TSV sidecar\n",
                temporal_support_sidecar_path_->c_str());
            return false;
        }

        std::fprintf(stderr, "temporal-support detail written to %s\n", temporal_support_sidecar_path_->c_str());
        return true;
    }

    auto print_temporal_support(temporal_support_summary_t const& summary) const -> void
    {
        std::fprintf(stdout, "\ntemporal support of normalized visible spans\n");
        std::fprintf(stdout, "  fixed block size: %zu observed reports\n", temporal_support_block_size);
        std::fprintf(stdout, "  blocks: %zu\n", summary.blocks.size());
        std::fprintf(stdout, "  minimum samples: minimum=1, p0.1=1000, p1=100, p5=20, p10=10\n");

        print_temporal_statistic_stability(summary);
        print_temporal_period_thresholds(summary);
        print_common_intercept_stability(summary);
        print_block_duration_stability(summary);
        print_worst_case_blocks(summary);

        std::fprintf(stdout, "\ndisjoint normalized visible-span distributions (us per report)\n");
        std::fprintf(stdout, "  windows restart at fixed block and timestamp-chain boundaries\n");

        print_visible_span_table_header();

        for (auto lag_index = std::size_t{0}; lag_index < temporal_support_lags.size(); ++lag_index)
        {
            print_visible_span_summary_row(temporal_support_lags[lag_index], summary.disjoint[lag_index]);
        }
    }

    std::optional<long double> expected_rate_hz_;
    std::optional<long double> expected_period_ns_;
    std::optional<std::string> temporal_support_sidecar_path_;

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

    std::size_t current_chain_report_begin_ = 0;
    std::size_t current_chain_delta_begin_ = 0;
    std::vector<delta_chain_t> delta_chains_;
    std::vector<temporal_block_timing_t> temporal_block_timings_;

    std::vector<std::uint64_t> histogram_counts_;
    std::uint64_t histogram_overflow_count_ = 0;

    std::optional<timestamp_pair_t> minimum_delta_;
    std::optional<timestamp_pair_t> minimum_positive_delta_;

    std::optional<delta_context_t> pending_delta_;
    std::vector<delta_context_t> shortest_delta_contexts_;

    std::optional<timestamp_regression_t> first_timestamp_regression_;
    std::optional<sequence_discontinuity_t> first_sequence_discontinuity_;
};

[[nodiscard]] auto run_stats(char const* path, std::optional<long double> expected_rate_hz) -> int
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
    auto stats = timestamp_stats_t{expected_rate_hz};
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
        stats.observe(result->value());
    }

    if (stop_requested)
    {
        complete = false;
        std::fprintf(stderr, "analysis interrupted; reported statistics are partial\n");
    }

    auto printed = stats.print();

    return complete && printed ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace
} // namespace crv

auto main(int argc, char** argv) -> int
{
    using namespace crv;

    if (argc < 2 || 3 < argc)
    {
        std::fprintf(stderr, "usage: %s CAPTURE_FILE [EXPECTED_POLL_RATE_HZ]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (!install_signal_handlers())
    {
        std::fprintf(stderr, "failed to install signal handlers: %s\n", std::strerror(errno));
        return EXIT_FAILURE;
    }

    auto expected_rate_hz = std::optional<long double>{};

    if (argc == 3)
    {
        expected_rate_hz = parse_expected_rate(argv[2]);

        if (!expected_rate_hz)
        {
            std::fprintf(stderr, "invalid expected polling rate: %s\n", argv[2]);
            return EXIT_FAILURE;
        }
    }

    return run_stats(argv[1], expected_rate_hz);
}
