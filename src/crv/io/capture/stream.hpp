// SPDX-License-Identifier: MIT

/// \file
/// \brief decoded raw linux input-value capture stream
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/io/unique_fd.hpp>
#include <crv/kernel/input/abi.h>
#include <crv/kernel/input/capture/abi.h>

#include <array>
#include <cassert>
#include <cerrno>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
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
/// The source owns the descriptor.
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

    truncated_stream_header,
    invalid_magic,
    unsupported_format_version,
    invalid_stream_header_size,
    stream_header_too_large,
    unsupported_input_value_size,
    unsupported_clock,
    unsupported_byte_order,
    unsupported_stream_flags,

    truncated_frame,
    invalid_frame_size,
    invalid_frame_header_size,
    frame_too_large,

    invalid_input_values_header_size,
    invalid_input_value_count,
    invalid_input_value_capacity,
    input_value_capacity_too_large,
    inconsistent_input_values_frame_size,
};

struct capture_stream_error_t
{
    capture_stream_error_code_t code;
    int system_error = 0;

    /// offset of the next unread byte when the error was detected
    std::uint64_t stream_offset = 0;
};

/// limits applied while decoding an untrusted or damaged stream
struct capture_stream_limits_t
{
    std::size_t maximum_stream_header_size = 4096;
    std::size_t maximum_frame_size = 2u * 1024u * 1024u;
    std::size_t maximum_input_value_capacity = 4096;
};

/// one captured input-handler events() callback
///
/// values remains valid until the next call to read_input_values() on the originating stream.
struct capture_input_values_view_t
{
    std::uint64_t timestamp_ns;
    std::uint64_t sequence;
    std::uint32_t value_capacity;
    std::span<crv_input_value_t const> values;
};

namespace generic {

/// decodes a complete capture protocol from an arbitrary byte source
///
/// The stream begins with crv_capture_stream_header_t and is followed by framed records. Unknown frame types are
/// skipped using their declared sizes. Input-values frames are returned as immutable callback views.
template <capture_source source_t> class capture_stream_t
{
public:
    using read_result_t = std::expected<std::optional<capture_input_values_view_t>, capture_stream_error_t>;

    explicit capture_stream_t(source_t source, capture_stream_limits_t limits = {})
        : source_{std::move(source)}, limits_{limits}
    {
        assert(limits_.maximum_stream_header_size >= sizeof(crv_capture_stream_header_t));
        assert(limits_.maximum_frame_size >= sizeof(crv_capture_input_values_header_t));
        assert(limits_.maximum_input_value_capacity != 0);
    }

    capture_stream_t(capture_stream_t const&) = delete;
    auto operator=(capture_stream_t const&) -> capture_stream_t& = delete;

    capture_stream_t(capture_stream_t&&) = default;
    auto operator=(capture_stream_t&&) -> capture_stream_t& = default;

    /// reads the next input-values callback frame
    ///
    /// An empty optional means clean source exhaustion at a frame boundary. EINTR is returned rather than retried so
    /// a live caller can honor Ctrl-C. Partial header, frame, and value state is preserved across an interrupted call.
    [[nodiscard]] auto read_input_values() -> read_result_t
    {
        if (terminal_error_) return std::unexpected(*terminal_error_);
        if (end_reached_) return std::optional<capture_input_values_view_t>{};

        for (;;)
        {
            switch (state_)
            {
                case state_t::reading_stream_header:
                {
                    if (auto const result = require(fill_object(stream_header_, stream_header_offset_),
                            capture_stream_error_code_t::truncated_stream_header);
                        !result)
                    {
                        return propagate(result.error());
                    }

                    if (auto const error = validate_stream_header()) return propagate(*error);

                    skip_remaining_ = stream_header_.header_size - sizeof(stream_header_);

                    if (skip_remaining_ == 0)
                    {
                        header_valid_ = true;
                        state_ = state_t::reading_frame_header;
                    }
                    else
                    {
                        state_ = state_t::skipping_stream_header_extension;
                    }

                    break;
                }

                case state_t::skipping_stream_header_extension:
                {
                    if (auto const result
                        = require(discard(skip_remaining_), capture_stream_error_code_t::truncated_stream_header);
                        !result)
                    {
                        return propagate(result.error());
                    }

                    header_valid_ = true;
                    reset_frame_state();
                    state_ = state_t::reading_frame_header;

                    break;
                }

                case state_t::reading_frame_header:
                {
                    /*
                        This is the only state that does not route through require().

                        end and disconnected at offset 0 are meaningful here for clean exhaustion and clean disconnect
                        at a frame boundary, respectively, rather than truncation.
                    */
                    auto result = fill_object(frame_header_, frame_header_offset_);
                    if (!result) return propagate(result.error());

                    if (*result == fill_status_t::end)
                    {
                        if (frame_header_offset_ != 0)
                        {
                            return propagate(make_error(capture_stream_error_code_t::truncated_frame));
                        }

                        end_reached_ = true;
                        return std::optional<capture_input_values_view_t>{};
                    }

                    if (*result == fill_status_t::disconnected)
                    {
                        if (frame_header_offset_ != 0)
                        {
                            return propagate(make_error(capture_stream_error_code_t::truncated_frame, ENODEV));
                        }

                        return propagate(make_error(capture_stream_error_code_t::source_disconnected, ENODEV));
                    }

                    if (auto const error = validate_frame_header()) return propagate(*error);

                    if (frame_header_.frame_type == CRV_CAPTURE_FRAME_TYPE_INPUT_VALUES)
                    {
                        if (frame_header_.header_size < sizeof(crv_capture_input_values_header_t))
                        {
                            return propagate(make_error(capture_stream_error_code_t::invalid_input_values_header_size));
                        }

                        /*
                            Resuming header decode mid-object depends on the embedded frame header being the first
                            member, so the bytes already consumed as frame_header_ land at the front of
                            input_values_header_. The ABI header asserts the full struct layout, but that does not tie
                            the layout to this requirement; if the layouts ever diverge, this assert points here rather
                            than presenting as a corrupt decode.
                        */
                        static_assert(offsetof(crv_capture_input_values_header_t, frame) == 0);

                        input_values_header_ = {};
                        input_values_header_.frame = frame_header_;
                        input_values_header_offset_ = sizeof(frame_header_);
                        state_ = state_t::reading_input_values_header;
                    }
                    else
                    {
                        skip_remaining_ = frame_header_.frame_size - sizeof(frame_header_);
                        state_ = state_t::skipping_unknown_frame;
                    }

                    break;
                }

                case state_t::skipping_unknown_frame:
                {
                    if (auto const result
                        = require(discard(skip_remaining_), capture_stream_error_code_t::truncated_frame);
                        !result)
                    {
                        return propagate(result.error());
                    }

                    reset_frame_state();
                    state_ = state_t::reading_frame_header;

                    break;
                }

                case state_t::reading_input_values_header:
                {
                    if (auto const result = require(fill_object(input_values_header_, input_values_header_offset_),
                            capture_stream_error_code_t::truncated_frame);
                        !result)
                    {
                        return propagate(result.error());
                    }

                    if (auto const error = validate_input_values_header()) return propagate(*error);

                    skip_remaining_ = input_values_header_.frame.header_size - sizeof(input_values_header_);

                    if (skip_remaining_ != 0)
                    {
                        state_ = state_t::skipping_input_values_header_extension;
                        break;
                    }

                    begin_input_values();
                    break;
                }

                case state_t::skipping_input_values_header_extension:
                {
                    if (auto const result
                        = require(discard(skip_remaining_), capture_stream_error_code_t::truncated_frame);
                        !result)
                    {
                        return propagate(result.error());
                    }

                    begin_input_values();
                    break;
                }

                case state_t::reading_input_values:
                {
                    auto destination = std::as_writable_bytes(std::span{input_values_});

                    if (auto const result = require(
                            fill(destination, input_values_offset_), capture_stream_error_code_t::truncated_frame);
                        !result)
                    {
                        return propagate(result.error());
                    }

                    auto const view = capture_input_values_view_t{
                        .timestamp_ns = input_values_header_.timestamp_ns,
                        .sequence = input_values_header_.sequence,
                        .value_capacity = input_values_header_.value_capacity,
                        .values = input_values_,
                    };

                    reset_frame_state();
                    state_ = state_t::reading_frame_header;

                    return std::optional<capture_input_values_view_t>{view};
                }
            }
        }
    }

    /// returns the validated stream header after it has been completely consumed
    [[nodiscard]] auto header() const noexcept -> crv_capture_stream_header_t const*
    {
        return header_valid_ ? &stream_header_ : nullptr;
    }

    [[nodiscard]] auto stream_offset() const noexcept -> std::uint64_t { return stream_offset_; }

private:
    enum class state_t
    {
        reading_stream_header,
        skipping_stream_header_extension,
        reading_frame_header,
        skipping_unknown_frame,
        reading_input_values_header,
        skipping_input_values_header_extension,
        reading_input_values,
    };

    enum class fill_status_t
    {
        complete,
        end,
        disconnected,
    };

    using fill_result_t = std::expected<fill_status_t, capture_stream_error_t>;

    [[nodiscard]] auto make_error(capture_stream_error_code_t code, int system_error = 0) const
        -> capture_stream_error_t
    {
        return {
            .code = code,
            .system_error = system_error,
            .stream_offset = stream_offset_,
        };
    }

    /// records and returns an error; interrupted is transient and never latched
    [[nodiscard]] auto propagate(capture_stream_error_t error) -> read_result_t
    {
        if (error.code != capture_stream_error_code_t::interrupted) terminal_error_ = error;

        return std::unexpected(error);
    }

    /// maps anything short of a complete fill to a truncation error, carrying ENODEV for disconnects
    [[nodiscard]] auto require(fill_result_t result, capture_stream_error_code_t truncation_code) const
        -> std::expected<void, capture_stream_error_t>
    {
        if (!result) return std::unexpected(result.error());

        if (*result != fill_status_t::complete)
        {
            return std::unexpected(make_error(truncation_code, *result == fill_status_t::disconnected ? ENODEV : 0));
        }

        return {};
    }

    [[nodiscard]] auto fill(std::span<std::byte> destination, std::size_t& offset) -> fill_result_t
    {
        assert(offset <= destination.size());

        while (offset != destination.size())
        {
            auto const source_result = source_.read_some(destination.subspan(offset));

            switch (source_result.status)
            {
                case capture_source_read_status_t::data:
                    if (source_result.size == 0 || source_result.size > destination.size() - offset)
                    {
                        return std::unexpected(make_error(capture_stream_error_code_t::source_read_failed, EIO));
                    }

                    offset += source_result.size;
                    stream_offset_ += source_result.size;

                    break;

                case capture_source_read_status_t::end: return fill_status_t::end;

                case capture_source_read_status_t::interrupted:
                    return std::unexpected(make_error(capture_stream_error_code_t::interrupted,
                        source_result.system_error != 0 ? source_result.system_error : EINTR));

                case capture_source_read_status_t::disconnected: return fill_status_t::disconnected;

                case capture_source_read_status_t::failed:
                    return std::unexpected(make_error(capture_stream_error_code_t::source_read_failed,
                        source_result.system_error != 0 ? source_result.system_error : EIO));
            }
        }

        return fill_status_t::complete;
    }

    template <typename value_t> [[nodiscard]] auto fill_object(value_t& value, std::size_t& offset) -> fill_result_t
    {
        static_assert(std::is_trivially_copyable_v<value_t>);

        return fill(std::as_writable_bytes(std::span{&value, std::size_t{1}}), offset);
    }

    /// consumes and discards bytes through fill(), preserving partial progress across an interrupted call
    [[nodiscard]] auto discard(std::size_t& remaining) -> fill_result_t
    {
        while (remaining != 0)
        {
            auto const requested = remaining < discard_buffer_.size() ? remaining : discard_buffer_.size();
            auto offset = std::size_t{0};
            auto const result = fill(std::span<std::byte>{discard_buffer_}.first(requested), offset);

            /*
                fill() advances offset for bytes consumed before any interrupt or short read, so decrementing
                unconditionally keeps remaining accurate when the caller resumes.
            */
            remaining -= offset;

            if (!result || *result != fill_status_t::complete) return result;
        }

        return fill_status_t::complete;
    }

    [[nodiscard]] auto validate_stream_header() const -> std::optional<capture_stream_error_t>
    {
        if (std::memcmp(stream_header_.magic, CRV_CAPTURE_STREAM_MAGIC, sizeof(stream_header_.magic)) != 0)
        {
            return make_error(capture_stream_error_code_t::invalid_magic);
        }

        /*
            Check byte order before interpreting the remaining multibyte fields. A stream written with the opposite
            native byte order would otherwise appear to have nonsense version and size values.
        */
        if (stream_header_.byte_order_marker != CRV_CAPTURE_BYTE_ORDER_MARKER)
        {
            return make_error(capture_stream_error_code_t::unsupported_byte_order);
        }

        if (stream_header_.format_version != CRV_CAPTURE_FORMAT_VERSION)
        {
            return make_error(capture_stream_error_code_t::unsupported_format_version);
        }

        if (stream_header_.header_size < sizeof(stream_header_))
        {
            return make_error(capture_stream_error_code_t::invalid_stream_header_size);
        }

        if (stream_header_.header_size > limits_.maximum_stream_header_size)
        {
            return make_error(capture_stream_error_code_t::stream_header_too_large);
        }

        if (stream_header_.input_value_size != sizeof(crv_input_value_t))
        {
            return make_error(capture_stream_error_code_t::unsupported_input_value_size);
        }

        if (stream_header_.clock_id != CRV_CAPTURE_CLOCK_MONOTONIC)
        {
            return make_error(capture_stream_error_code_t::unsupported_clock);
        }

        if (stream_header_.flags != 0) return make_error(capture_stream_error_code_t::unsupported_stream_flags);

        return std::nullopt;
    }

    [[nodiscard]] auto validate_frame_header() const -> std::optional<capture_stream_error_t>
    {
        if (frame_header_.frame_size < sizeof(frame_header_))
        {
            return make_error(capture_stream_error_code_t::invalid_frame_size);
        }

        if (frame_header_.header_size < sizeof(frame_header_) || frame_header_.header_size > frame_header_.frame_size)
        {
            return make_error(capture_stream_error_code_t::invalid_frame_header_size);
        }

        if (frame_header_.frame_size > limits_.maximum_frame_size)
        {
            return make_error(capture_stream_error_code_t::frame_too_large);
        }

        return std::nullopt;
    }

    [[nodiscard]] auto validate_input_values_header() const -> std::optional<capture_stream_error_t>
    {
        auto const value_count = static_cast<std::size_t>(input_values_header_.value_count);
        auto const value_capacity = static_cast<std::size_t>(input_values_header_.value_capacity);

        // this is technically already validated in reading_frame_header, but the paths may diverge
        if (input_values_header_.frame.header_size < sizeof(input_values_header_))
        {
            return make_error(capture_stream_error_code_t::invalid_input_values_header_size);
        }

        if (value_count == 0) return make_error(capture_stream_error_code_t::invalid_input_value_count);

        if (value_capacity == 0 || value_count > value_capacity)
        {
            return make_error(capture_stream_error_code_t::invalid_input_value_capacity);
        }

        if (value_capacity > limits_.maximum_input_value_capacity)
        {
            return make_error(capture_stream_error_code_t::input_value_capacity_too_large);
        }

        if (value_count > std::numeric_limits<std::size_t>::max() / sizeof(crv_input_value_t))
        {
            return make_error(capture_stream_error_code_t::inconsistent_input_values_frame_size);
        }

        auto const value_bytes = value_count * sizeof(crv_input_value_t);
        auto const header_size = static_cast<std::size_t>(input_values_header_.frame.header_size);

        if (header_size > std::numeric_limits<std::size_t>::max() - value_bytes)
        {
            return make_error(capture_stream_error_code_t::inconsistent_input_values_frame_size);
        }

        if (header_size + value_bytes != input_values_header_.frame.frame_size)
        {
            return make_error(capture_stream_error_code_t::inconsistent_input_values_frame_size);
        }

        return std::nullopt;
    }

    void begin_input_values()
    {
        input_values_.resize(input_values_header_.value_count);
        input_values_offset_ = 0;
        state_ = state_t::reading_input_values;
    }

    void reset_frame_state()
    {
        frame_header_ = {};
        frame_header_offset_ = 0;

        input_values_header_ = {};
        input_values_header_offset_ = 0;
        input_values_offset_ = 0;

        skip_remaining_ = 0;
    }

    source_t source_;
    capture_stream_limits_t limits_;

    state_t state_ = state_t::reading_stream_header;
    std::uint64_t stream_offset_ = 0;

    crv_capture_stream_header_t stream_header_{};
    std::size_t stream_header_offset_ = 0;
    bool header_valid_ = false;

    crv_capture_frame_header_t frame_header_{};
    std::size_t frame_header_offset_ = 0;

    crv_capture_input_values_header_t input_values_header_{};
    std::size_t input_values_header_offset_ = 0;

    std::vector<crv_input_value_t> input_values_;
    std::size_t input_values_offset_ = 0;

    std::size_t skip_remaining_ = 0;
    std::array<std::byte, 4096> discard_buffer_{};

    bool end_reached_ = false;
    std::optional<capture_stream_error_t> terminal_error_;
};

} // namespace generic

using capture_stream_t = generic::capture_stream_t<fd_capture_source_t>;

} // namespace crv
