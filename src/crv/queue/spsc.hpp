// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief fixed-capacity atomic single-producer/single-consumer queue
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <array>
#include <atomic>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

namespace crv {

/// fixed-capacity atomic single-producer/single-consumer queue
///
/// Producer and consumer must each be externally serialized. They may run concurrently with one another. reset()
/// requires external exclusion of both endpoints.
///
/// Capacity is measured in records. Records are copied by object representation, so record_t must be non-cv and
/// trivially copyable, default constructible, and destructible.
///
/// Writes are all-or-nothing. Reads may partially succeed because the destination copier may short-copy. Successfully
/// copied records are consumed.
template <typename record_t, std::size_t capacity> class spsc_t
{
public:
    using position_t = std::uint32_t;
    using span_t = std::span<record_t const>;

    static constexpr auto cache_line_size = std::size_t{64};

    static_assert(!std::is_const_v<record_t>);
    static_assert(!std::is_volatile_v<record_t>);
    static_assert(std::is_trivially_copyable_v<record_t>);
    static_assert(std::is_trivially_default_constructible_v<record_t>);
    static_assert(std::is_trivially_destructible_v<record_t>);
    static_assert(capacity >= 2);
    static_assert((capacity & (capacity - 1)) == 0);
    static_assert(capacity < (std::size_t{1} << 31));
    static_assert(capacity <= std::numeric_limits<std::size_t>::max() / sizeof(record_t));
    static_assert(std::atomic<position_t>::is_always_lock_free);
    static_assert(sizeof(std::atomic<position_t>) == sizeof(position_t));

    constexpr spsc_t() noexcept : spsc_t{0} {}

    /// constructs an empty queue whose logical positions begin at initial_position
    ///
    /// A nonzero initial position is useful for exercising unsigned counter rollover without transferring 2^32
    /// records.
    explicit constexpr spsc_t(position_t initial_position) noexcept
        : producer_{initial_position}, consumer_{initial_position}
    {}

    spsc_t(spsc_t const&) = delete;
    auto operator=(spsc_t const&) -> spsc_t& = delete;
    spsc_t(spsc_t&&) = delete;
    auto operator=(spsc_t&&) -> spsc_t& = delete;

    /// attempts to append one record
    ///
    /// \returns true if the record was appended; false otherwise
    [[nodiscard]] auto try_write(record_t const& record) noexcept -> bool { return try_write(span_t{&record, 1}); }

    /// attempts to append one contiguous record span
    ///
    /// \returns true if the complete span was appended; false otherwise
    [[nodiscard]] auto try_write(span_t records) noexcept -> bool { return try_write(records, {}); }

    /// attempts to append two record spans as one logical write
    ///
    /// A successful two-span write publishes both spans together with one release store.
    /// Both spans are copied before the new producer position is published. A consumer therefore observes either
    /// the complete combined write or none of it.
    ///
    /// For a std::byte queue, this is a common pattern when writing a frame header and frame content.
    ///
    /// \returns true if both complete spans were appended; false otherwise
    [[nodiscard]] auto try_write(span_t first, span_t second) noexcept -> bool
    {
        // written this way rather than first.size() + second.size() > capacity so size_t overflow is impossible
        if (first.size() > capacity || second.size() > capacity - first.size()) return false;

        auto const record_count = first.size() + second.size();
        if (!record_count) return true;

        // A stale cached consumer position only underestimates writable space.
        // Refresh it only when the cached value cannot admit the complete logical write.
        if (writable_size_from_cache() < record_count)
        {
            producer_.cached_peer_position = consumer_.published_position.load(std::memory_order_acquire);

            if (writable_size_from_cache() < record_count) return false;
        }

        write(producer_.position, first);
        write(producer_.position + static_cast<position_t>(first.size()), second);

        producer_.position += static_cast<position_t>(record_count);
        producer_.published_position.store(producer_.position, std::memory_order_release);

        return true;
    }

    /// copies and consumes at most maximum records
    ///
    /// copier(destination_offset, source) receives one contiguous physical source span and returns the number of
    /// records copied from it. The copier must not throw, and its returned count must not exceed source.size(). It is
    /// called at most twice when the readable range wraps around the physical end of the queue.
    ///
    /// A short copy stops the read. Successfully copied records are consumed, and the consumer position is published
    /// at most once per read().
    ///
    /// For record types larger than one byte, the copier reports only complete records. If an underlying byte copy
    /// stops within a record, the copier must round down; the incomplete record remains readable on the next call.
    ///
    /// Bytes written beyond the returned complete-record count are not part of this read's reported result and must not
    /// be consumed from the stream.
    ///
    /// \returns the number of records copied and consumed
    template <typename copier_t>
        requires requires(copier_t& copier, std::size_t destination_offset, span_t source) {
            { copier(destination_offset, source) } noexcept -> std::same_as<std::size_t>;
        }
    [[nodiscard]] auto read(std::size_t maximum, copier_t&& copier) noexcept -> std::size_t
    {
        if (!maximum) return 0;

        auto readable = readable_size_from_cache();

        // A stale cached producer position only underestimates readable data.
        // Refresh it when the cached range would otherwise make this read short.
        // If the cache already satisfies maximum, newly published records cannot improve this operation.
        if (maximum > readable)
        {
            consumer_.cached_peer_position = producer_.published_position.load(std::memory_order_acquire);
            readable = readable_size_from_cache();
        }

        if (!readable) return 0;

        auto const requested = maximum < readable ? maximum : readable;
        auto const range = split(consumer_.position, requested);

        auto copied = copy_span(copier, 0, span_t{storage_.data() + range.offset, range.first});

        if (copied == range.first && range.second)
        {
            copied += copy_span(copier, copied, span_t{storage_.data(), range.second});
        }

        publish_consumed(copied);
        return copied;
    }

    /// advisory lockless observation for polling and wait predicates
    ///
    /// The result may become stale immediately. When the caller excludes consumer advancement, it may transition
    /// from true to false, but not false to true. When the caller excludes producer advancement, it may transition
    /// from false to true, but not true to false. A caller racing both endpoints has no directional guarantee.
    ///
    /// This must not replace the authoritative read-side check.
    [[nodiscard]] auto empty() const noexcept -> bool
    {
        // Relaxed loads are sufficient for the advisory contract above. Keep acquire ordering until the polling and
        // wait-queue integration exists and confirms that no caller relies on empty() as a synchronization point.
        auto const tail = consumer_.published_position.load(std::memory_order_acquire);
        auto const head = producer_.published_position.load(std::memory_order_acquire);
        return head == tail;
    }

    /// resets the queue to empty at logical position zero
    ///
    /// The caller must externally exclude both producer and consumer endpoints.
    auto reset() noexcept -> void
    {
        producer_.position = 0;
        producer_.cached_peer_position = 0;
        producer_.published_position.store(0, std::memory_order_relaxed);

        consumer_.position = 0;
        consumer_.cached_peer_position = 0;
        consumer_.published_position.store(0, std::memory_order_relaxed);
    }

private:
    struct alignas(cache_line_size) endpoint_t
    {
        explicit constexpr endpoint_t(position_t initial_position) noexcept
            : position{initial_position}, cached_peer_position{initial_position}, published_position{initial_position}
        {}

        position_t position;
        position_t cached_peer_position;
        std::atomic<position_t> published_position;
    };

    struct split_t
    {
        std::size_t offset;
        std::size_t first;
        std::size_t second;
    };

    static constexpr auto mask = capacity - 1;

    static_assert(sizeof(endpoint_t) == cache_line_size);

    /// splits a logical range into at most two contiguous physical ranges
    ///
    /// \pre count <= capacity
    [[nodiscard]] static constexpr auto split(position_t position, std::size_t count) noexcept -> split_t
    {
        auto const offset = static_cast<std::size_t>(position) & mask;
        auto const first = count < capacity - offset ? count : capacity - offset;
        return {.offset = offset, .first = first, .second = count - first};
    }

    [[nodiscard]] auto writable_size_from_cache() const noexcept -> std::size_t
    {
        auto const used = static_cast<position_t>(producer_.position - producer_.cached_peer_position);
        assert(static_cast<std::size_t>(used) <= capacity);
        return capacity - static_cast<std::size_t>(used);
    }

    [[nodiscard]] auto readable_size_from_cache() const noexcept -> std::size_t
    {
        auto const readable = static_cast<position_t>(consumer_.cached_peer_position - consumer_.position);
        assert(static_cast<std::size_t>(readable) <= capacity);
        return static_cast<std::size_t>(readable);
    }

    auto write(position_t position, span_t records) noexcept -> void
    {
        if (records.empty()) return;

        auto const range = split(position, records.size());

        __builtin_memcpy(storage_.data() + range.offset, records.data(), range.first * sizeof(record_t));

        if (range.second)
        {
            __builtin_memcpy(storage_.data(), records.data() + range.first, range.second * sizeof(record_t));
        }
    }

    template <typename copier_t>
    [[nodiscard]] static auto copy_span(copier_t& copier, std::size_t destination_offset, span_t source) noexcept
        -> std::size_t
    {
        auto const copied = copier(destination_offset, source);
        assert(copied <= source.size());
        return copied;
    }

    auto publish_consumed(std::size_t copied) noexcept -> void
    {
        if (!copied) return;

        consumer_.position += static_cast<position_t>(copied);
        consumer_.published_position.store(consumer_.position, std::memory_order_release);
    }

    endpoint_t producer_;
    endpoint_t consumer_;
    alignas(cache_line_size) std::array<record_t, capacity> storage_{};
};

} // namespace crv
