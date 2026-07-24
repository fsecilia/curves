// SPDX-License-Identifier: GPL-2.0+ OR MIT

#include "spsc_test.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <gmock/gmock.h>
#include <limits>
#include <span>
#include <vector>

namespace crv {
namespace {

struct spsc_byte_unit_test_t : spsc_byte_test_t
{
    using sut_t = spsc_t<8>;
};

TEST_F(spsc_byte_unit_test_t, single_span_write_is_read_back)
{
    auto sut = sut_t{};
    auto const bytes = make_bytes(8, 0x20u);

    ASSERT_TRUE(sut.try_write(bytes));

    auto [copied, actual] = read_bytes(sut, 8);

    EXPECT_EQ(copied, bytes.size());
    EXPECT_EQ(actual, bytes);
    EXPECT_TRUE(sut.empty());
}

TEST_F(spsc_byte_unit_test_t, two_span_write_is_read_back_as_one_stream)
{
    auto sut = sut_t{};
    advance_empty_stream_to(sut, 6);

    auto const first = make_bytes(3, 0x30u);
    auto const second = make_bytes(5, 0x70u);
    ASSERT_TRUE(sut.try_write(first, second));

    auto [copied, actual] = read_bytes(sut, 8);
    auto expected = first;
    expected.insert(expected.end(), second.begin(), second.end());

    EXPECT_EQ(copied, expected.size());
    EXPECT_EQ(actual, expected);
    EXPECT_TRUE(sut.empty());
}

TEST_F(spsc_byte_unit_test_t, rejected_write_does_not_modify_contents)
{
    auto sut = sut_t{};
    auto const existing = make_bytes(6, 0x40u);
    auto const rejected = make_bytes(3, 0x80u);

    ASSERT_TRUE(sut.try_write(existing));
    EXPECT_FALSE(sut.try_write(rejected));

    auto [copied, actual] = read_bytes(sut, 8);

    EXPECT_EQ(copied, existing.size());
    EXPECT_EQ(actual, existing);
}

TEST_F(spsc_byte_unit_test_t, read_refreshes_cached_producer_position_before_returning_short)
{
    auto sut = sut_t{};
    auto const first = make_bytes(4, 0x21u);
    auto const second = make_bytes(4, 0x81u);

    ASSERT_TRUE(sut.try_write(first));

    auto [prefix_copied, prefix] = read_bytes(sut, 2);
    auto const expected_prefix = std::vector<std::byte>{first.begin(), first.begin() + 2};

    ASSERT_EQ(prefix_copied, expected_prefix.size());
    ASSERT_EQ(prefix, expected_prefix);

    ASSERT_TRUE(sut.try_write(second));

    auto [copied, actual] = read_bytes(sut, 6);
    auto expected = std::vector<std::byte>{first.begin() + 2, first.end()};
    expected.insert(expected.end(), second.begin(), second.end());

    EXPECT_EQ(copied, expected.size());
    EXPECT_EQ(actual, expected);
    EXPECT_TRUE(sut.empty());
}

TEST_F(spsc_byte_unit_test_t, reset_discards_contents_and_cached_positions)
{
    auto sut = sut_t{};
    auto const before_reset = make_bytes(7, 0x70u);
    ASSERT_TRUE(sut.try_write(before_reset));

    auto [prefix_copied, prefix] = read_bytes(sut, 3);
    ASSERT_EQ(prefix_copied, 3);
    ASSERT_EQ(prefix.size(), 3);

    sut.reset();
    EXPECT_TRUE(sut.empty());

    auto const after_reset = make_bytes(8, 0x90u);
    EXPECT_TRUE(sut.try_write(after_reset));

    auto [copied, actual] = read_bytes(sut, 8);
    EXPECT_EQ(copied, after_reset.size());
    EXPECT_EQ(actual, after_reset);
}

TEST_F(spsc_byte_unit_test_t, initial_position_exercises_uint32_rollover)
{
    auto const origin = std::numeric_limits<std::uint32_t>::max() - 3u;
    auto sut = sut_t{origin};

    auto const first = make_bytes(6, 0xa0u);
    ASSERT_TRUE(sut.try_write(first));

    auto [first_copied, first_actual] = read_bytes(sut, 8);
    EXPECT_EQ(first_copied, first.size());
    EXPECT_EQ(first_actual, first);
    EXPECT_TRUE(sut.empty());

    auto const second = make_bytes(8, 0xb0u);
    ASSERT_TRUE(sut.try_write(second));

    auto [second_copied, second_actual] = read_bytes(sut, 8);
    EXPECT_EQ(second_copied, second.size());
    EXPECT_EQ(second_actual, second);
    EXPECT_TRUE(sut.empty());
}

struct spsc_byte_unit_mock_test_t : spsc_byte_unit_test_t
{
    struct mock_copier_t
    {
        virtual ~mock_copier_t() = default;
        MOCK_METHOD(std::size_t, call, (std::size_t, std::byte const*, std::size_t), (noexcept));
    };
    StrictMock<mock_copier_t> mock_copier;

    struct copier_t
    {
        mock_copier_t* mock = nullptr;
        auto operator()(std::size_t offset, std::span<std::byte const> source) noexcept -> std::size_t
        {
            return mock->call(offset, source.data(), source.size());
        }
    };
};

TEST_F(spsc_byte_unit_mock_test_t, first_physical_short_copy_consumes_only_copied_bytes)
{
    auto sut = sut_t{};
    auto const bytes = make_bytes(6, 0x40u);
    ASSERT_TRUE(sut.try_write(bytes));

    EXPECT_CALL(mock_copier, call(0, _, 6)).WillOnce(Return(4));

    auto const copied = sut.read(6, copier_t{&mock_copier});

    EXPECT_EQ(copied, 4);

    auto [remainder_copied, remainder] = read_bytes(sut, 8);
    auto const expected = std::vector<std::byte>{bytes.begin() + 4, bytes.end()};

    EXPECT_EQ(remainder_copied, expected.size());
    EXPECT_EQ(remainder, expected);
}

TEST_F(spsc_byte_unit_mock_test_t, wrapped_second_short_copy_consumes_first_span_and_copied_prefix)
{
    auto sut = sut_t{};
    advance_empty_stream_to(sut, 6);

    auto const bytes = make_bytes(6, 0x50u);
    ASSERT_TRUE(sut.try_write(bytes));

    {
        auto const seq = InSequence{};

        EXPECT_CALL(mock_copier, call(0, _, 2))
            .WillOnce(Invoke([&](std::size_t, std::byte const* source, std::size_t size) noexcept -> std::size_t {
                EXPECT_TRUE(std::equal(source, source + size, bytes.begin()));
                return size;
            }));

        EXPECT_CALL(mock_copier, call(2, _, 4))
            .WillOnce(Invoke([&](std::size_t, std::byte const* source, std::size_t size) noexcept -> std::size_t {
                EXPECT_TRUE(std::equal(source, source + size, bytes.begin() + 2));
                return 2;
            }));
    }

    auto const copied = sut.read(6, copier_t{&mock_copier});

    EXPECT_EQ(copied, 4);

    auto [remainder_copied, remainder] = read_bytes(sut, 8);
    auto const expected = std::vector<std::byte>{bytes.begin() + 4, bytes.end()};

    EXPECT_EQ(remainder_copied, expected.size());
    EXPECT_EQ(remainder, expected);
}

TEST_F(spsc_byte_unit_mock_test_t, zero_length_read_and_write_are_no_ops)
{
    auto sut = sut_t{};

    EXPECT_TRUE(sut.try_write(sut_t::span_t{}));
    EXPECT_TRUE(sut.try_write({}, {}));
    EXPECT_TRUE(sut.empty());

    EXPECT_CALL(mock_copier, call(_, _, _)).Times(0);
    EXPECT_EQ(sut.read(0, copier_t{&mock_copier}), 0);
    EXPECT_TRUE(sut.empty());
}

struct spsc_record_unit_test_t : spsc_record_test_t
{
    using sut_t = spsc_t<8>;
};

TEST_F(spsc_record_unit_test_t, single_record_write_is_read_back)
{
    auto sut = sut_t{};
    auto const record = make_record(0x1234u);

    ASSERT_TRUE(sut.try_write(record));

    auto [copied, actual] = read_records(sut, 1);

    EXPECT_EQ(copied, 1);
    EXPECT_EQ(actual, std::vector<record_t>{record});
    EXPECT_TRUE(sut.empty());
}

TEST_F(spsc_record_unit_test_t, record_spans_wrap_without_byte_misalignment)
{
    auto sut = sut_t{};
    advance_empty_queue_to(sut, 6);

    auto const first = make_records(3, 0x200u);
    auto const second = make_records(5, 0x300u);
    ASSERT_TRUE(sut.try_write(first, second));

    auto [copied, actual] = read_records(sut, 8);
    auto expected = first;
    expected.insert(expected.end(), second.begin(), second.end());

    EXPECT_EQ(copied, expected.size());
    EXPECT_EQ(actual, expected);
    EXPECT_TRUE(sut.empty());
}

TEST_F(spsc_record_unit_test_t, capacity_is_measured_in_records)
{
    auto sut = spsc_t<4>{};
    auto const records = make_records(4, 0x600u);

    ASSERT_TRUE(sut.try_write(records));
    EXPECT_FALSE(sut.try_write(make_record(0x700u)));

    auto [copied, actual] = read_records(sut, 4);

    EXPECT_EQ(copied, records.size());
    EXPECT_EQ(actual, records);
}

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST_F(spsc_byte_unit_test_t, copier_cannot_report_more_than_source_size)
{
    auto sut = sut_t{};
    auto const bytes = make_bytes(4, 0x60u);
    ASSERT_TRUE(sut.try_write(bytes));

    EXPECT_DEBUG_DEATH(
        {
            auto copier = [](std::size_t, std::span<std::byte const> source) noexcept -> std::size_t {
                return source.size() + 1;
            };
            static_cast<void>(sut.read(4, copier));
        },
        "");
}

#endif

struct spsc_record_unit_mock_test_t : spsc_record_unit_test_t
{
    struct mock_copier_t
    {
        virtual ~mock_copier_t() = default;
        MOCK_METHOD(std::size_t, call, (std::size_t, record_t const*, std::size_t), (noexcept));
    };
    StrictMock<mock_copier_t> mock_copier;

    struct copier_t
    {
        mock_copier_t* mock = nullptr;
        auto operator()(std::size_t offset, std::span<record_t const> source) noexcept -> std::size_t
        {
            return mock->call(offset, source.data(), source.size());
        }
    };
};

TEST_F(spsc_record_unit_mock_test_t, wrapped_read_reports_destination_offsets_in_records)
{
    auto sut = sut_t{};
    advance_empty_queue_to(sut, 6);

    auto const records = make_records(6, 0x500u);
    ASSERT_TRUE(sut.try_write(records));

    {
        auto const seq = InSequence{};

        EXPECT_CALL(mock_copier, call(0, _, 2))
            .WillOnce(Invoke([&](std::size_t, record_t const* source, std::size_t size) noexcept -> std::size_t {
                EXPECT_TRUE(std::equal(source, source + size, records.begin()));
                return size;
            }));

        EXPECT_CALL(mock_copier, call(2, _, 4))
            .WillOnce(Invoke([&](std::size_t, record_t const* source, std::size_t size) noexcept -> std::size_t {
                EXPECT_TRUE(std::equal(source, source + size, records.begin() + 2));
                return size;
            }));
    }

    EXPECT_EQ(sut.read(6, copier_t{&mock_copier}), 6);
    EXPECT_TRUE(sut.empty());
}

TEST_F(spsc_record_unit_mock_test_t, short_copy_consumes_only_complete_records)
{
    auto sut = sut_t{};
    auto const records = make_records(6, 0x400u);
    ASSERT_TRUE(sut.try_write(records));

    EXPECT_CALL(mock_copier, call(0, _, 6)).WillOnce(Return(4));

    auto const copied = sut.read(6, copier_t{&mock_copier});

    EXPECT_EQ(copied, 4);

    auto [remainder_copied, remainder] = read_records(sut, 8);
    auto const expected = std::vector<record_t>{records.begin() + 4, records.end()};

    EXPECT_EQ(remainder_copied, expected.size());
    EXPECT_EQ(remainder, expected);
}

} // namespace
} // namespace crv
