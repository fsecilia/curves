// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "record_copier.hpp"
#include <crv/test/test.hpp>
#include <array>
#include <cstdint>
#include <gmock/gmock.h>
#include <limits>
#include <span>
#include <vector>

namespace crv {
namespace {

struct record_copier_test_t : Test
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

        auto operator()(std::size_t offset, std::span<std::byte const> source) noexcept -> std::size_t
        {
            return mock->call(offset, source.data(), source.size());
        }
    };

    struct record_t
    {
        std::uint32_t sequence;
        std::uint32_t inverse;
        std::int16_t x;
        std::int16_t y;

        auto operator==(record_t const&) const -> bool = default;
    };

    struct byte_copier_spy_state_t
    {
        std::size_t copied_bytes_to_return{};
        std::size_t call_count{};
        std::size_t destination_offset{};
        std::byte const* source_data{};
        std::size_t source_size{};
    };

    struct byte_copier_spy_t
    {
        byte_copier_spy_state_t* state = nullptr;

        auto operator()(std::size_t destination_offset, std::span<std::byte const> source) noexcept -> std::size_t
        {
            ++state->call_count;
            state->destination_offset = destination_offset;
            state->source_data = source.data();
            state->source_size = source.size();

            return state->copied_bytes_to_return;
        }
    };

    struct fixed_result_byte_copier_t
    {
        std::size_t copied_bytes{};

        auto operator()(std::size_t, std::span<std::byte const>) noexcept -> std::size_t { return copied_bytes; }
    };

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

    using sut_t = record_copier_t<record_t, byte_copier_t>;
    sut_t sut{{&mock_byte_copier}};
};

static_assert(std::is_trivially_copyable_v<record_copier_test_t::record_t>);
static_assert(is_byte_copier<record_copier_test_t::byte_copier_t>);
static_assert(is_byte_copier<record_copier_test_t::byte_copier_spy_t>);

struct potentially_throwing_byte_copier_t
{
    auto operator()(std::size_t, std::span<std::byte const>) -> std::size_t;
};

struct wrong_result_byte_copier_t
{
    auto operator()(std::size_t, std::span<std::byte const>) noexcept -> bool;
};

static_assert(!is_byte_copier<potentially_throwing_byte_copier_t>);
static_assert(!is_byte_copier<wrong_result_byte_copier_t>);

template <typename record_t, typename copier_t>
concept can_form_record_copier = requires { typename record_copier_t<record_t, copier_t>; };

struct nontrivial_record_t
{
    ~nontrivial_record_t() {}
};

static_assert(!can_form_record_copier<nontrivial_record_t, record_copier_test_t::byte_copier_t>);

TEST_F(record_copier_test_t, initially_has_no_short_copy)
{
    EXPECT_FALSE(sut.short_copy());
}

TEST_F(record_copier_test_t, complete_copy_forwards_the_byte_offset_and_exact_source_span)
{
    auto const records = make_records(6, 0x100u);
    auto const source = std::span<record_t const>{records}.subspan(1, 4);
    auto const source_bytes = std::as_bytes(source);

    EXPECT_CALL(mock_byte_copier, call(3 * sizeof(record_t), source_bytes.data(), source_bytes.size()))
        .WillOnce(Return(source_bytes.size()));

    EXPECT_EQ(sut(3, source), source.size());
    EXPECT_FALSE(sut.short_copy());
}

TEST_F(record_copier_test_t, empty_source_is_a_complete_copy)
{
    auto const source = std::span<record_t const>{};

    EXPECT_CALL(mock_byte_copier, call(7 * sizeof(record_t), _, 0)).WillOnce(Return(0));

    EXPECT_EQ(sut(7, source), 0);
    EXPECT_FALSE(sut.short_copy());
}

TEST_F(record_copier_test_t, zero_byte_copy_of_nonempty_source_reports_no_records)
{
    auto const records = make_records(4, 0x200u);

    EXPECT_CALL(mock_byte_copier, call(0, _, records.size() * sizeof(record_t))).WillOnce(Return(0));

    EXPECT_EQ(sut(0, records), 0);
    EXPECT_TRUE(sut.short_copy());
}

TEST_F(record_copier_test_t, record_aligned_short_copy_reports_the_completed_records)
{
    auto const records = make_records(4, 0x300u);

    EXPECT_CALL(mock_byte_copier, call(sizeof(record_t), _, records.size() * sizeof(record_t)))
        .WillOnce(Return(2 * sizeof(record_t)));

    EXPECT_EQ(sut(1, records), 2);
    EXPECT_TRUE(sut.short_copy());
}

TEST_F(record_copier_test_t, partial_record_is_rounded_down)
{
    auto const records = make_records(4, 0x400u);

    EXPECT_CALL(mock_byte_copier, call(2 * sizeof(record_t), _, records.size() * sizeof(record_t)))
        .WillOnce(Return(2 * sizeof(record_t) + 5));

    EXPECT_EQ(sut(2, records), 2);
    EXPECT_TRUE(sut.short_copy());
}

TEST_F(record_copier_test_t, every_possible_byte_count_reports_only_complete_records)
{
    auto const records = make_records(4, 0x500u);
    auto const source = std::span<record_t const>{records};
    auto const source_bytes = std::as_bytes(source);
    constexpr auto destination_offset = std::size_t{11};

    for (auto copied_bytes = std::size_t{}; copied_bytes <= source_bytes.size(); ++copied_bytes)
    {
        SCOPED_TRACE(copied_bytes);

        auto state = byte_copier_spy_state_t{
            .copied_bytes_to_return = copied_bytes,
        };

        auto local_sut = record_copier_t<record_t, byte_copier_spy_t>{byte_copier_spy_t{&state}};

        EXPECT_EQ(local_sut(destination_offset, source), copied_bytes / sizeof(record_t));
        EXPECT_EQ(local_sut.short_copy(), copied_bytes != source_bytes.size());

        EXPECT_EQ(state.call_count, 1);
        EXPECT_EQ(state.destination_offset, destination_offset * sizeof(record_t));
        EXPECT_EQ(state.source_data, source_bytes.data());
        EXPECT_EQ(state.source_size, source_bytes.size());
    }
}

TEST_F(record_copier_test_t, short_copy_state_is_sticky_after_a_later_complete_copy)
{
    auto const records = make_records(3, 0x600u);
    auto const source_bytes = std::as_bytes(std::span<record_t const>{records});

    InSequence sequence;

    EXPECT_CALL(mock_byte_copier, call(0, source_bytes.data(), source_bytes.size())).WillOnce(Return(sizeof(record_t)));

    EXPECT_CALL(mock_byte_copier, call(0, source_bytes.data(), source_bytes.size()))
        .WillOnce(Return(source_bytes.size()));

    EXPECT_EQ(sut(0, records), 1);
    EXPECT_TRUE(sut.short_copy());

    EXPECT_EQ(sut(0, records), records.size());
    EXPECT_TRUE(sut.short_copy());
}

TEST_F(record_copier_test_t, repeated_complete_copies_do_not_set_short_copy)
{
    auto const records = make_records(2, 0x700u);
    auto const source_bytes = std::as_bytes(std::span<record_t const>{records});

    EXPECT_CALL(mock_byte_copier, call(0, source_bytes.data(), source_bytes.size()))
        .Times(2)
        .WillRepeatedly(Return(source_bytes.size()));

    EXPECT_EQ(sut(0, records), records.size());
    EXPECT_FALSE(sut.short_copy());

    EXPECT_EQ(sut(0, records), records.size());
    EXPECT_FALSE(sut.short_copy());
}

TEST_F(record_copier_test_t, largest_valid_destination_offset_is_scaled_without_overflow)
{
    auto const destination_offset = std::numeric_limits<std::size_t>::max() / sizeof(record_t);
    auto const destination_byte_offset = destination_offset * sizeof(record_t);
    auto const source = std::span<record_t const>{};

    EXPECT_CALL(mock_byte_copier, call(destination_byte_offset, _, 0)).WillOnce(Return(0));

    EXPECT_EQ(sut(destination_offset, source), 0);
    EXPECT_FALSE(sut.short_copy());
}

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST_F(record_copier_test_t, destination_offset_overflow_asserts)
{
    EXPECT_DEBUG_DEATH(([] {
        auto local_sut = record_copier_t<record_t, fixed_result_byte_copier_t>{fixed_result_byte_copier_t{0}};

        auto const invalid_offset = std::numeric_limits<std::size_t>::max() / sizeof(record_t) + 1;

        (void)local_sut(invalid_offset, std::span<record_t const>{});
    }()),
        "");
}

TEST_F(record_copier_test_t, byte_copier_overreport_asserts)
{
    EXPECT_DEBUG_DEATH(([] {
        auto const records = std::array<record_t, 2>{};

        auto local_sut
            = record_copier_t<record_t, fixed_result_byte_copier_t>{fixed_result_byte_copier_t{sizeof(records) + 1}};

        (void)local_sut(0, records);
    }()),
        "");
}

#endif

} // namespace
} // namespace crv
