// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

extern "C" {
#include <crv/kernel/input/abi.h>
#include <crv/kernel/input/capture/abi.h>
#include <crv/kernel/input/capture/device.h>
} // extern "C"

#include <crv/lib.hpp>
#include <crv/math/limits.hpp>
#include <concepts>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>

namespace crv {

template <typename sink_t>
concept exact_byte_sink = requires(sink_t& sink, std::span<std::byte const> bytes) {
    { sink.try_write_exact(bytes) } -> std::same_as<bool>;
};

[[nodiscard]] constexpr auto make_capture_stream_header() noexcept -> crv_capture_stream_header_t
{
    return crv_capture_stream_header_t{
        .magic = CRV_CAPTURE_STREAM_MAGIC,
        .abi_major = CRV_CAPTURE_ABI_MAJOR,
        .abi_minor = CRV_CAPTURE_ABI_MINOR,
        .stream_kind = CRV_CAPTURE_STREAM_RAW_INPUT_VALUES,
        .timestamp_kind = CRV_CAPTURE_TIMESTAMP_CALLBACK_MONOTONIC_NS,
        .header_size = sizeof(crv_capture_stream_header_t),
        .batch_header_size = sizeof(crv_capture_batch_header_t),
        .input_value_size = sizeof(crv_input_value_t),
        .flags = 0,
    };
}

[[nodiscard]] constexpr auto capture_batch_size(std::size_t count) noexcept -> std::optional<std::size_t>
{
    constexpr auto header_size = sizeof(crv_capture_batch_header_t);
    constexpr auto value_size = sizeof(crv_input_value_t);

    if (count > (max<std::size_t>() - header_size) / value_size) return std::nullopt;

    return header_size + count * value_size;
}

template <exact_byte_sink sink_t> class capture_stream_producer_t
{
public:
    constexpr capture_stream_producer_t(
        sink_t sink, std::span<std::byte> scratch, crv_capture_producer_state_t& state) noexcept
        : sink_{std::move(sink)}, scratch_{scratch}, state_{state}
    {}

    [[nodiscard]] auto begin_session() noexcept -> bool
    {
        state_ = {};

        auto const header = make_capture_stream_header();
        auto const bytes = std::as_bytes(std::span<crv_capture_stream_header_t const>{&header, 1});

        if (!sink_.try_write_exact(bytes)) return false;

        state_.bytes_written = bytes.size();
        return true;
    }

    [[nodiscard]] auto try_push(crv_capture_u64_t timestamp_ns, crv_input_value_t const* values, std::size_t count,
        std::size_t capacity) noexcept -> crv_capture_push_result_t
    {
        constexpr auto wire_count_max = max<crv_capture_u32_t>();

        if (count > capacity || count > wire_count_max || capacity > wire_count_max
            || (count != 0 && values == nullptr))
        {
            return CRV_CAPTURE_INVALID_INPUT;
        }

        auto const frame_size = capture_batch_size(count);
        if (!frame_size) return CRV_CAPTURE_INVALID_INPUT;
        if (*frame_size > scratch_.size()) return CRV_CAPTURE_SCRATCH_TOO_SMALL;

        // Structural validation happens before assigning a sequence. A valid observed callback consumes a sequence even
        // if transport is full.
        auto const sequence = state_.next_sequence++;

        auto const header = crv_capture_batch_header_t{
            .timestamp_ns = timestamp_ns,
            .sequence = sequence,
            .count = static_cast<crv_capture_u32_t>(count),
            .capacity = static_cast<crv_capture_u32_t>(capacity),
        };

        __builtin_memcpy(scratch_.data(), &header, sizeof(header));
        if (count != 0)
        {
            __builtin_memcpy(scratch_.data() + sizeof(header), values, count * sizeof(crv_input_value_t));
        }

        auto const frame = std::span<std::byte const>{scratch_.data(), *frame_size};
        if (!sink_.try_write_exact(frame))
        {
            ++state_.batches_dropped;
            return CRV_CAPTURE_QUEUE_FULL;
        }

        ++state_.batches_written;
        state_.bytes_written += frame.size();

        return CRV_CAPTURE_PUSHED;
    }

    [[nodiscard]] constexpr auto state() const noexcept -> crv_capture_producer_state_t const& { return state_; }

private:
    sink_t sink_;
    std::span<std::byte> scratch_;
    crv_capture_producer_state_t& state_;
};

} // namespace crv
