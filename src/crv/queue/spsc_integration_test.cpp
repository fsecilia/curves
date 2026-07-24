// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "spsc_test.hpp"
#include <crv/queue/record_copier.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <random>
#include <span>
#include <thread>
#include <vector>

namespace crv {
namespace {

struct spsc_byte_integration_test_t : spsc_byte_test_t
{};

TEST_F(spsc_byte_integration_test_t, randomized_operations_match_reference_model)
{
    constexpr auto capacity = std::size_t{32};
    constexpr auto seed = std::uint32_t{0x8ca35d17u};
    constexpr auto operation_count = std::size_t{250'000};

    auto sut = spsc_t<capacity>{};
    auto model = std::deque<std::byte>{};
    auto random = std::mt19937{seed};

    for (auto operation = std::size_t{}; operation < operation_count; ++operation)
    {
        SCOPED_TRACE(Message{} << "seed=" << seed << " operation=" << operation);

        if (random() & 1u)
        {
            auto const first_size = static_cast<std::size_t>(random() % (capacity + 9));
            auto const second_size = static_cast<std::size_t>(random() % (capacity + 9));
            auto const first = make_bytes(first_size, static_cast<std::uint32_t>(operation));
            auto const second = make_bytes(second_size, static_cast<std::uint32_t>(~operation));
            auto const writable = capacity - model.size();
            auto const expected_acceptance = first_size <= writable && second_size <= writable - first_size;

            EXPECT_EQ(sut.try_write(first, second), expected_acceptance);

            if (expected_acceptance)
            {
                model.insert(model.end(), first.begin(), first.end());
                model.insert(model.end(), second.begin(), second.end());
            }
        }
        else
        {
            auto const maximum = static_cast<std::size_t>(random() % (capacity + 9));
            auto [copied, actual] = read_bytes(sut, maximum);
            auto const expected_count = std::min(maximum, model.size());
            auto expected = std::vector<std::byte>{};
            expected.reserve(expected_count);

            for (auto i = std::size_t{}; i < expected_count; ++i)
            {
                expected.push_back(model.front());
                model.pop_front();
            }

            EXPECT_EQ(copied, expected_count);
            EXPECT_EQ(actual, expected);
        }

        EXPECT_EQ(sut.empty(), model.empty());
    }
}

struct spsc_byte_integration_exhaustive_test_single_span_write_t : spsc_byte_integration_test_t
{
    template <std::size_t capacity> auto run() -> void
    {
        for (auto start = std::size_t{}; start < capacity; ++start)
        {
            for (auto occupancy = std::size_t{}; occupancy <= capacity; ++occupancy)
            {
                for (auto write_size = std::size_t{}; write_size <= capacity + 1; ++write_size)
                {
                    SCOPED_TRACE(Message{} << "capacity=" << capacity << " start=" << start
                                           << " occupancy=" << occupancy << " write_size=" << write_size);

                    auto sut = spsc_t<capacity>{};
                    advance_empty_stream_to(sut, start);

                    auto const existing = make_bytes(occupancy, 0x20u);
                    ASSERT_TRUE(sut.try_write(existing));

                    auto const incoming = make_bytes(write_size, 0x80u);
                    auto const expected_acceptance = write_size <= capacity - occupancy;

                    EXPECT_EQ(sut.try_write(incoming), expected_acceptance);

                    auto expected = existing;
                    if (expected_acceptance) { expected.insert(expected.end(), incoming.begin(), incoming.end()); }

                    auto [copied, actual] = read_bytes(sut, capacity);

                    EXPECT_EQ(copied, expected.size());
                    EXPECT_EQ(actual, expected);
                    EXPECT_TRUE(sut.empty());
                }
            }
        }
    }
};

TEST_F(spsc_byte_integration_exhaustive_test_single_span_write_t, exhaustive_single_span_writes)
{
    run<2>();
    run<4>();
    run<8>();
    run<16>();
    run<32>();
}

struct spsc_byte_integration_exhaustive_test_double_span_write_t : spsc_byte_integration_test_t
{
    template <std::size_t capacity> auto run() -> void
    {
        for (auto start = std::size_t{}; start < capacity; ++start)
        {
            for (auto occupancy = std::size_t{}; occupancy <= capacity; ++occupancy)
            {
                for (auto first_size = std::size_t{}; first_size <= capacity + 1; ++first_size)
                {
                    for (auto second_size = std::size_t{}; second_size <= capacity + 1; ++second_size)
                    {
                        SCOPED_TRACE(Message{} << "capacity=" << capacity << " start=" << start
                                               << " occupancy=" << occupancy << " first_size=" << first_size
                                               << " second_size=" << second_size);

                        auto sut = spsc_t<capacity>{};
                        advance_empty_stream_to(sut, start);

                        auto const existing = make_bytes(occupancy, 0x20u);
                        ASSERT_TRUE(sut.try_write(existing));

                        auto const first = make_bytes(first_size, 0x60u);
                        auto const second = make_bytes(second_size, 0xa0u);
                        auto const writable = capacity - occupancy;
                        auto const expected_acceptance = first_size <= writable && second_size <= writable - first_size;

                        EXPECT_EQ(sut.try_write(first, second), expected_acceptance);

                        auto expected = existing;
                        if (expected_acceptance)
                        {
                            expected.insert(expected.end(), first.begin(), first.end());
                            expected.insert(expected.end(), second.begin(), second.end());
                        }

                        auto [copied, actual] = read_bytes(sut, capacity);

                        EXPECT_EQ(copied, expected.size());
                        EXPECT_EQ(actual, expected);
                        EXPECT_TRUE(sut.empty());
                    }
                }
            }
        }
    }
};

TEST_F(spsc_byte_integration_exhaustive_test_double_span_write_t, exhaustive_two_span_writes)
{
    run<2>();
    run<4>();
    run<8>();
    run<16>();
}

struct spsc_byte_integration_exhaustive_test_read_t : spsc_byte_integration_test_t
{
    template <std::size_t capacity> auto run() -> void
    {
        for (auto start = std::size_t{}; start < capacity; ++start)
        {
            for (auto occupancy = std::size_t{}; occupancy <= capacity; ++occupancy)
            {
                for (auto maximum = std::size_t{}; maximum <= capacity + 1; ++maximum)
                {
                    SCOPED_TRACE(Message{} << "capacity=" << capacity << " start=" << start
                                           << " occupancy=" << occupancy << " maximum=" << maximum);

                    auto sut = spsc_t<capacity>{};
                    advance_empty_stream_to(sut, start);

                    auto const existing = make_bytes(occupancy, 0x30u);
                    ASSERT_TRUE(sut.try_write(existing));

                    auto [copied, prefix] = read_bytes(sut, maximum);
                    auto const expected_count = std::min(maximum, occupancy);
                    auto const expected_prefix
                        = std::vector<std::byte>{existing.begin(), existing.begin() + expected_count};

                    EXPECT_EQ(copied, expected_count);
                    EXPECT_EQ(prefix, expected_prefix);

                    auto [remainder_copied, remainder] = read_bytes(sut, capacity);
                    auto const expected_remainder
                        = std::vector<std::byte>{existing.begin() + expected_count, existing.end()};

                    EXPECT_EQ(remainder_copied, expected_remainder.size());
                    EXPECT_EQ(remainder, expected_remainder);
                    EXPECT_TRUE(sut.empty());
                }
            }
        }
    }
};

TEST_F(spsc_byte_integration_exhaustive_test_read_t, exhaustive_reads)
{
    run<2>();
    run<4>();
    run<8>();
    run<16>();
    run<32>();
}

struct spsc_byte_integration_thread_test_t : spsc_byte_integration_test_t
{
    struct record_t
    {
        std::array<std::byte, 13> first;
        std::array<std::byte, 31> second;
        std::size_t first_size;
        std::size_t second_size;
    };

    auto make_record(std::uint32_t sequence) -> record_t
    {
        auto record = record_t{};

        record.first_size = sequence % (record.first.size() + 1);
        record.second_size = (sequence * 17u + 3u) % (record.second.size() + 1);

        if (!record.first_size && !record.second_size) record.second_size = 1;

        for (auto i = std::size_t{}; i < record.first_size; ++i)
        {
            record.first[i] = std::byte{static_cast<unsigned char>((sequence + 29u * i) & 0xffu)};
        }

        for (auto i = std::size_t{}; i < record.second_size; ++i)
        {
            record.second[i] = std::byte{static_cast<unsigned char>((sequence * 7u + 43u * i + 0x5au) & 0xffu)};
        }

        return record;
    }

    auto append_record(std::vector<std::byte>& dst, record_t const& record) -> void
    {
        dst.insert(dst.end(), record.first.begin(), record.first.begin() + record.first_size);
        dst.insert(dst.end(), record.second.begin(), record.second.begin() + record.second_size);
    }
};

TEST_F(spsc_byte_integration_thread_test_t, randomly_chunked_stream)
{
    using sut_t = spsc_t<256>;

    constexpr auto record_count = std::uint32_t{100'000};
    constexpr auto timeout = std::chrono::seconds{30};
    constexpr auto producer_seed = std::uint32_t{0x6d2b79f5u};
    constexpr auto consumer_seed = std::uint32_t{0x9e3779b9u};

    auto expected = std::vector<std::byte>{};
    expected.reserve(static_cast<std::size_t>(record_count) * 24);

    for (auto sequence = std::uint32_t{}; sequence < record_count; ++sequence)
    {
        append_record(expected, make_record(sequence));
    }

    auto actual = std::vector<std::byte>(expected.size());
    auto sut = sut_t{};
    auto failure = std::atomic<int>{0};
    auto const deadline = std::chrono::steady_clock::now() + timeout;

    auto producer = std::thread{[&]() -> void {
        auto random = std::mt19937{producer_seed};

        for (auto sequence = std::uint32_t{}; sequence < record_count; ++sequence)
        {
            auto const record = make_record(sequence);
            auto const first = std::span<std::byte const>{record.first.data(), record.first_size};
            auto const second = std::span<std::byte const>{record.second.data(), record.second_size};

            while (!sut.try_write(first, second))
            {
                if (failure.load(std::memory_order_relaxed)) return;

                if ((random() & 7u) == 0) std::this_thread::yield();

                if (std::chrono::steady_clock::now() >= deadline)
                {
                    failure.store(1, std::memory_order_relaxed);
                    return;
                }
            }
        }
    }};

    auto consumer = std::thread{[&]() -> void {
        auto written = std::size_t{};
        auto random = std::mt19937{consumer_seed};

        while (written < actual.size())
        {
            if (failure.load(std::memory_order_relaxed)) return;

            auto const maximum = std::size_t{1} + random() % 97u;
            auto const copied = sut.read(
                maximum, [&](std::size_t dst_offset, std::span<std::byte const> src) noexcept -> std::size_t {
                    if (written + dst_offset + src.size() > actual.size())
                    {
                        failure.store(2, std::memory_order_relaxed);
                        return std::size_t{};
                    }

                    std::memcpy(actual.data() + written + dst_offset, src.data(), src.size());
                    return src.size();
                });

            written += copied;

            if (!copied)
            {
                std::this_thread::yield();

                if (std::chrono::steady_clock::now() >= deadline)
                {
                    failure.store(3, std::memory_order_relaxed);
                    return;
                }
            }
        }
    }};

    producer.join();
    consumer.join();

    ASSERT_EQ(failure.load(std::memory_order_relaxed), 0);
    EXPECT_EQ(actual, expected);
    EXPECT_TRUE(sut.empty());
}

struct spsc_record_integration_test_t : spsc_record_test_t
{};

TEST_F(spsc_record_integration_test_t, randomized_record_operations_match_reference_model)
{
    constexpr auto capacity = std::size_t{32};
    constexpr auto seed = std::uint32_t{0x4e91c7a3u};
    constexpr auto operation_count = std::size_t{100'000};

    auto sut = spsc_t<capacity>{};
    auto model = std::deque<record_t>{};
    auto random = std::mt19937{seed};

    for (auto operation = std::size_t{}; operation < operation_count; ++operation)
    {
        SCOPED_TRACE(Message{} << "seed=" << seed << " operation=" << operation);

        if (random() & 1u)
        {
            auto const first_size = static_cast<std::size_t>(random() % (capacity + 9));
            auto const second_size = static_cast<std::size_t>(random() % (capacity + 9));
            auto const first = make_records(first_size, static_cast<std::uint32_t>(operation));
            auto const second = make_records(second_size, static_cast<std::uint32_t>(~operation));
            auto const writable = capacity - model.size();
            auto const expected_acceptance = first_size <= writable && second_size <= writable - first_size;

            EXPECT_EQ(sut.try_write(first, second), expected_acceptance);

            if (expected_acceptance)
            {
                model.insert(model.end(), first.begin(), first.end());
                model.insert(model.end(), second.begin(), second.end());
            }
        }
        else
        {
            auto const maximum = static_cast<std::size_t>(random() % (capacity + 9));
            auto [copied, actual] = read_records(sut, maximum);
            auto const expected_count = std::min(maximum, model.size());
            auto expected = std::vector<record_t>{};
            expected.reserve(expected_count);

            for (auto i = std::size_t{}; i < expected_count; ++i)
            {
                expected.push_back(model.front());
                model.pop_front();
            }

            EXPECT_EQ(copied, expected_count);
            EXPECT_EQ(actual, expected);
        }

        EXPECT_EQ(sut.empty(), model.empty());
    }
}

struct spsc_whole_record_integration_test_t : spsc_record_test_t
{
    struct mock_byte_copier_t
    {
        virtual ~mock_byte_copier_t() = default;
        MOCK_METHOD(std::size_t, call, (std::size_t, std::byte const*, std::size_t), (noexcept));
    };
    StrictMock<mock_byte_copier_t> mock_byte_copier;

    struct byte_copier_t
    {
        mock_byte_copier_t* mock = nullptr;
        auto operator()(std::size_t offset, std::span<std::byte const> src) noexcept -> std::size_t
        {
            return mock->call(offset, src.data(), src.size());
        }
    };

    using sut_t = spsc_t<8>;
};

TEST_F(spsc_whole_record_integration_test_t, partial_first_record_is_not_consumed)
{
    auto sut = sut_t{};
    auto const records = make_records(4, 0x480u);
    ASSERT_TRUE(sut.try_write(records));

    EXPECT_CALL(mock_byte_copier, call(0, _, records.size() * sizeof(record_t))).WillOnce(Return(sizeof(record_t) - 1));

    auto copier = record_copier_t<record_t, byte_copier_t>{{&mock_byte_copier}};

    EXPECT_EQ(sut.read(records.size(), copier), 0);

    auto [copied, actual] = read_records(sut, records.size());

    EXPECT_EQ(copied, records.size());
    EXPECT_EQ(actual, records);
    EXPECT_TRUE(sut.empty());
}

TEST_F(spsc_whole_record_integration_test_t, wrapped_partial_record_consumes_only_complete_records)
{
    auto sut = sut_t{};
    advance_empty_queue_to(sut, 6);

    auto const records = make_records(6, 0x580u);
    ASSERT_TRUE(sut.try_write(records));

    {
        auto const seq = InSequence{};

        // two complete records before the physical wrap
        EXPECT_CALL(mock_byte_copier, call(0, _, 2 * sizeof(record_t))).WillOnce(Return(2 * sizeof(record_t)));

        // one complete record and a partial following record after the wrap
        EXPECT_CALL(mock_byte_copier, call(2 * sizeof(record_t), _, 4 * sizeof(record_t)))
            .WillOnce(Return(sizeof(record_t) + 5));
    }

    auto copier = record_copier_t<record_t, byte_copier_t>{{&mock_byte_copier}};

    EXPECT_EQ(sut.read(records.size(), copier), 3);

    auto [remainder_copied, remainder] = read_records(sut, records.size());
    auto const expected = std::vector<record_t>{records.begin() + 3, records.end()};

    EXPECT_EQ(remainder_copied, expected.size());
    EXPECT_EQ(remainder, expected);
    EXPECT_TRUE(sut.empty());
}

} // namespace
} // namespace crv
