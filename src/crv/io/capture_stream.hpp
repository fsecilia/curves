// SPDX-License-Identifier: MIT

/// \file
/// \brief decoded stream of raw linux input-handler callback batches
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/io/unique_fd.hpp>
#include <crv/kernel/input/abi.h>
#include <crv/kernel/input/capture_abi.h>
#include <array>
#include <cassert>
#include <cerrno>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <optional>
#include <span>
#include <type_traits>
#include <unistd.h>
#include <utility>
#include <vector>

namespace crv {

/// result of one attempt to obtain bytes from a capture source
enum class capture_source_read_status_t
{
    data,
    end,
    interrupted,
    disconnected,
    failed,
};

struct capture_source_read_result_t
{
    capture_source_read_status_t status;
    std::size_t size = 0;
    int system_error = 0;
};

template <typename source_t>
concept capture_source = requires(source_t& source, std::span<std::byte> destination) {
    { source.read_some(destination) } -> std::same_as<capture_source_read_result_t>;
};

/// capture source backed by any readable posix file descriptor
///
/// The source owns the descriptor. A regular file and /dev/crv-input-capture therefore feed the same decoder.
class fd_capture_source_t
{
public:
    explicit fd_capture_source_t(unique_fd_t fd) noexcept : fd_{std::move(fd)} { assert(fd_); }

    [[nodiscard]] auto read_some(std::span<std::byte> destination) -> capture_source_read_result_t
    {
        assert(!destination.empty());

        auto const result = read(fd_.get(), destination.data(), destination.size());

        if (result > 0)
        {
            return {
                .status = capture_source_read_status_t::data,
                .size = static_cast<std::size_t>(result),
            };
        }

        if (result == 0) return {.status = capture_source_read_status_t::end};

        if (errno == EINTR)
        {
            return {
                .status = capture_source_read_status_t::interrupted,
                .system_error = EINTR,
            };
        }

        if (errno == ENODEV)
        {
            return {
                .status = capture_source_read_status_t::disconnected,
                .system_error = ENODEV,
            };
        }

        return {
            .status = capture_source_read_status_t::failed,
            .system_error = errno != 0 ? errno : EIO,
        };
    }

private:
    unique_fd_t fd_;
};

enum class capture_stream_error_code_t
{
    interrupted,
    source_disconnected,
    source_read_failed,
    truncated_record,
    inconsistent_batch_timestamp,
    batch_too_large,
};

struct capture_stream_error_t
{
    capture_stream_error_code_t code;
    int system_error = 0;
};

/// one input-handler events() callback decoded into the input abi consumed by the scanner
///
/// values remains valid until the next call to read_batch() on the originating stream.
struct capture_batch_view_t
{
    std::uint64_t timestamp_ns;
    std::uint64_t batch_sequence;
    std::span<crv_input_value_t> values;
};

namespace generic {

/// groups raw capture events from an arbitrary byte source into callback batches.
///
/// This layer knows nothing about files, headers, sequence continuity, SYN_REPORT placement, duplicate axes, or other
/// input-report semantics. It only reconstructs fixed-size records and groups adjacent records with equal sequence IDs.
template <capture_source source_t> class capture_stream_t
{
public:
    using read_result_t = std::expected<std::optional<capture_batch_view_t>, capture_stream_error_t>;

    static constexpr auto default_max_batch_value_count = std::size_t{4096};
    static constexpr auto buffered_event_count = std::size_t{4096};

    explicit capture_stream_t(source_t source, std::size_t max_batch_value_count = default_max_batch_value_count)
        : source_{std::move(source)}, max_batch_value_count_{max_batch_value_count}
    {
        values_.reserve(max_batch_value_count_);
    }

    capture_stream_t(capture_stream_t const&) = delete;
    auto operator=(capture_stream_t const&) -> capture_stream_t& = delete;

    capture_stream_t(capture_stream_t&&) noexcept = default;
    auto operator=(capture_stream_t&&) noexcept -> capture_stream_t& = default;

    /// reads one complete input-handler callback
    ///
    /// An empty optional means clean source exhaustion. EINTR is returned as interrupted rather than retried so a live
    /// caller can honor Ctrl-C. If the caller elects to retry, partially assembled record and batch state is preserved.
    [[nodiscard]] auto read_batch() -> read_result_t
    {
        if (terminal_error_) return std::unexpected(*terminal_error_);
        if (end_reached_ && !assembling_batch_) return std::optional<capture_batch_view_t>{};

        if (!assembling_batch_)
        {
            values_.clear();

            auto first_result = read_event();

            if (!first_result)
            {
                auto const error = first_result.error();
                if (error.code != capture_stream_error_code_t::interrupted) terminal_error_ = error;
                return std::unexpected(error);
            }

            if (!*first_result)
            {
                end_reached_ = true;
                return std::optional<capture_batch_view_t>{};
            }

            auto const append_result = begin_batch(**first_result);
            if (!append_result)
            {
                terminal_error_ = append_result.error();
                return std::unexpected(*terminal_error_);
            }
        }

        for (;;)
        {
            auto event_result = read_event();

            if (!event_result)
            {
                auto const error = event_result.error();

                if (error.code == capture_stream_error_code_t::interrupted)
                {
                    // preserve assembling_batch_, values_, and any partial record bytes so a retry resumes exactly
                    return std::unexpected(error);
                }

                if (error.code == capture_stream_error_code_t::source_disconnected)
                {
                    // The kernel reports ENODEV only after its buffered records have been drained. Deliver the final
                    // assembled callback first, then report the sticky disconnect on the following call.
                    terminal_error_ = error;
                    return finish_batch();
                }

                terminal_error_ = error;
                return std::unexpected(*terminal_error_);
            }

            if (!*event_result)
            {
                end_reached_ = true;
                return finish_batch();
            }

            auto const& event = **event_result;

            if (event.batch_sequence != batch_sequence_)
            {
                pending_event_ = event;
                return finish_batch();
            }

            if (event.timestamp_ns != timestamp_ns_)
            {
                terminal_error_ = capture_stream_error_t{
                    .code = capture_stream_error_code_t::inconsistent_batch_timestamp,
                };
                return std::unexpected(*terminal_error_);
            }

            auto const append_result = append(event);
            if (!append_result)
            {
                terminal_error_ = append_result.error();
                return std::unexpected(*terminal_error_);
            }
        }
    }

private:
    using event_read_result_t = std::expected<std::optional<crv_capture_event_t>, capture_stream_error_t>;
    using append_result_t = std::expected<void, capture_stream_error_t>;

    [[nodiscard]] auto begin_batch(crv_capture_event_t const& event) -> append_result_t
    {
        assert(!assembling_batch_);

        timestamp_ns_ = event.timestamp_ns;
        batch_sequence_ = event.batch_sequence;
        assembling_batch_ = true;

        return append(event);
    }

    [[nodiscard]] auto append(crv_capture_event_t const& event) -> append_result_t
    {
        if (values_.size() >= max_batch_value_count_)
        {
            return std::unexpected(capture_stream_error_t{
                .code = capture_stream_error_code_t::batch_too_large,
            });
        }

        values_.push_back(crv_input_value_t{
            .type = event.type,
            .code = event.code,
            .value = event.value,
        });

        return {};
    }

    [[nodiscard]] auto finish_batch() -> read_result_t
    {
        assert(assembling_batch_);
        assembling_batch_ = false;

        return std::optional<capture_batch_view_t>{capture_batch_view_t{
            .timestamp_ns = timestamp_ns_,
            .batch_sequence = batch_sequence_,
            .values = values_,
        }};
    }

    [[nodiscard]] auto read_event() -> event_read_result_t
    {
        if (pending_event_)
        {
            auto const event = *pending_event_;
            pending_event_.reset();
            return std::optional<crv_capture_event_t>{event};
        }

        static_assert(std::is_trivially_copyable_v<crv_capture_event_t>);

        constexpr auto record_size = sizeof(crv_capture_event_t);

        for (;;)
        {
            auto const available = buffer_end_ - buffer_begin_;

            if (available >= record_size)
            {
                auto event = crv_capture_event_t{};
                std::memcpy(&event, byte_buffer_.data() + buffer_begin_, record_size);
                buffer_begin_ += record_size;

                if (buffer_begin_ == buffer_end_)
                {
                    buffer_begin_ = 0;
                    buffer_end_ = 0;
                }

                return std::optional<crv_capture_event_t>{event};
            }

            if (buffer_begin_ != 0)
            {
                if (available != 0) std::memmove(byte_buffer_.data(), byte_buffer_.data() + buffer_begin_, available);

                buffer_begin_ = 0;
                buffer_end_ = available;
            }

            auto destination = std::span<std::byte>{byte_buffer_}.subspan(buffer_end_);
            assert(!destination.empty());

            auto const source_result = source_.read_some(destination);

            switch (source_result.status)
            {
                case capture_source_read_status_t::data:
                    if (source_result.size == 0 || source_result.size > destination.size())
                    {
                        return std::unexpected(capture_stream_error_t{
                            .code = capture_stream_error_code_t::source_read_failed,
                            .system_error = EIO,
                        });
                    }

                    buffer_end_ += source_result.size;
                    break;

                case capture_source_read_status_t::end:
                    if (buffer_end_ != 0)
                    {
                        return std::unexpected(capture_stream_error_t{
                            .code = capture_stream_error_code_t::truncated_record,
                        });
                    }

                    return std::optional<crv_capture_event_t>{};

                case capture_source_read_status_t::interrupted:
                    return std::unexpected(capture_stream_error_t{
                        .code = capture_stream_error_code_t::interrupted,
                        .system_error = source_result.system_error != 0 ? source_result.system_error : EINTR,
                    });

                case capture_source_read_status_t::disconnected:
                    if (buffer_end_ != 0)
                    {
                        return std::unexpected(capture_stream_error_t{
                            .code = capture_stream_error_code_t::truncated_record,
                        });
                    }

                    return std::unexpected(capture_stream_error_t{
                        .code = capture_stream_error_code_t::source_disconnected,
                        .system_error = source_result.system_error != 0 ? source_result.system_error : ENODEV,
                    });

                case capture_source_read_status_t::failed:
                    return std::unexpected(capture_stream_error_t{
                        .code = capture_stream_error_code_t::source_read_failed,
                        .system_error = source_result.system_error != 0 ? source_result.system_error : EIO,
                    });
            }
        }
    }

    source_t source_;
    std::size_t max_batch_value_count_;

    std::array<std::byte, buffered_event_count * sizeof(crv_capture_event_t)> byte_buffer_{};
    std::size_t buffer_begin_ = 0;
    std::size_t buffer_end_ = 0;

    std::optional<crv_capture_event_t> pending_event_;
    std::vector<crv_input_value_t> values_;

    bool assembling_batch_ = false;
    bool end_reached_ = false;
    std::uint64_t timestamp_ns_ = 0;
    std::uint64_t batch_sequence_ = 0;

    std::optional<capture_stream_error_t> terminal_error_;
};

} // namespace generic

using capture_stream_t = generic::capture_stream_t<fd_capture_source_t>;

} // namespace crv
