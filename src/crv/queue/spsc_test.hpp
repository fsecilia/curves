// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief common mechanisms shared between spsc unit and integration tests
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include "spsc.hpp"
#include <crv/test/test.hpp>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gmock/gmock.h>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace crv {
namespace {

struct spsc_test_t : Test
{
    template <typename record_t> struct vector_copier_t
    {
        std::span<record_t> destination;

        auto operator()(std::size_t offset, std::span<record_t const> source) noexcept -> std::size_t
        {
            assert(offset <= destination.size());
            assert(source.size() <= destination.size() - offset);

            if (!source.empty()) { std::memcpy(destination.data() + offset, source.data(), source.size_bytes()); }

            return source.size();
        }
    };

    template <typename record_t> struct read_records_t
    {
        std::size_t count;
        std::vector<record_t> records;
    };

    template <typename record_t, std::size_t capacity>
    auto read_records(crv::spsc_t<record_t, capacity>& queue, std::size_t maximum) -> read_records_t<record_t>
    {
        auto records = std::vector<record_t>(maximum);
        auto copier = vector_copier_t<record_t>{records};
        auto const copied = queue.read(maximum, copier);
        records.resize(copied);
        return {copied, std::move(records)};
    }
};

struct spsc_byte_test_t : spsc_test_t
{
    template <std::size_t capacity> using spsc_t = crv::spsc_t<std::byte, capacity>;
    using vector_copier_t = spsc_test_t::vector_copier_t<std::byte>;

    auto make_bytes(std::size_t count, std::uint32_t salt) -> std::vector<std::byte>
    {
        auto bytes = std::vector<std::byte>(count);

        for (auto i = std::size_t{}; i < count; ++i)
        {
            auto const value = static_cast<unsigned char>((salt + 37u * i + 11u * i * i) & 0xffu);
            bytes[i] = std::byte{value};
        }

        return bytes;
    }

    template <std::size_t capacity>
    auto read_bytes(spsc_t<capacity>& queue, std::size_t maximum) -> read_records_t<std::byte>
    {
        return read_records(queue, maximum);
    }

    template <std::size_t capacity> auto advance_empty_stream_to(spsc_t<capacity>& queue, std::size_t offset) -> void
    {
        auto const padding = make_bytes(offset, 0x10u);
        ASSERT_TRUE(queue.try_write(padding));

        auto [copied, bytes] = read_bytes(queue, offset);

        ASSERT_EQ(copied, offset);
        ASSERT_EQ(bytes, padding);
        ASSERT_TRUE(queue.empty());
    }
};

struct spsc_record_test_t : spsc_test_t
{
    struct record_t
    {
        std::uint32_t sequence;
        std::uint32_t inverse;
        std::int16_t x;
        std::int16_t y;

        auto operator==(record_t const&) const -> bool = default;
    };

    static_assert(sizeof(record_t) == 12);
    static_assert(std::is_trivially_copyable_v<record_t>);
    static_assert(std::is_trivially_default_constructible_v<record_t>);
    static_assert(std::is_trivially_destructible_v<record_t>);

    template <std::size_t capacity> using spsc_t = crv::spsc_t<record_t, capacity>;
    using vector_copier_t = spsc_test_t::vector_copier_t<record_t>;

    auto make_record(std::uint32_t sequence) -> record_t
    {
        return {
            .sequence = sequence,
            .inverse = ~sequence,
            .x = static_cast<std::int16_t>((sequence * 17u + 3u) & 0x7fffu),
            .y = static_cast<std::int16_t>((sequence * 29u + 5u) & 0x7fffu),
        };
    }

    auto make_records(std::size_t count, std::uint32_t salt) -> std::vector<record_t>
    {
        auto records = std::vector<record_t>{};
        records.reserve(count);

        for (auto i = std::size_t{}; i < count; ++i)
        {
            records.push_back(make_record(salt + static_cast<std::uint32_t>(i)));
        }

        return records;
    }

    template <std::size_t capacity> auto advance_empty_queue_to(spsc_t<capacity>& queue, std::size_t offset) -> void
    {
        auto const padding = make_records(offset, 0x100u);
        ASSERT_TRUE(queue.try_write(padding));

        auto [copied, records] = read_records(queue, offset);

        ASSERT_EQ(copied, offset);
        ASSERT_EQ(records, padding);
        ASSERT_TRUE(queue.empty());
    }
};

} // namespace
} // namespace crv
