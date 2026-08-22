// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "input_frame.hpp"
#include <crv/test/test.hpp>
#include <array>
#include <concepts>
#include <cstddef>

namespace crv::pipeline {
namespace {

constexpr auto record(input_value_t::type_t type, input_value_t::code_t code, input_value_t::value_t value) noexcept
    -> input_value_t
{
    return {.type = type, .code = code, .value = value};
}

constexpr auto record(input_value_t::type_t type, input_value_t::code_syn_t code, input_value_t::value_t value) noexcept
    -> input_value_t
{
    return record(type, static_cast<input_value_t::code_t>(code), value);
}

constexpr auto syn_report(input_value_t::value_t value = 0) noexcept -> input_value_t
{
    return record(input_value_t::type_t::syn, input_value_t::code_syn_t::report, value);
}

// arbitrary records
constexpr auto event_a
    = record(static_cast<input_value_t::type_t>(0x01), static_cast<input_value_t::code_t>(0x0100), 11);
constexpr auto event_b
    = record(static_cast<input_value_t::type_t>(0x01), static_cast<input_value_t::code_t>(0x0101), 22);
constexpr auto event_c
    = record(static_cast<input_value_t::type_t>(0x04), static_cast<input_value_t::code_t>(0x0200), 33);
constexpr auto event_d
    = record(static_cast<input_value_t::type_t>(0x04), static_cast<input_value_t::code_t>(0x0201), 44);
constexpr auto event_e
    = record(static_cast<input_value_t::type_t>(0x05), static_cast<input_value_t::code_t>(0x0300), 55);
constexpr auto event_f
    = record(static_cast<input_value_t::type_t>(0x05), static_cast<input_value_t::code_t>(0x0301), 66);

constexpr auto canary_a
    = record(static_cast<input_value_t::type_t>(0x7ffe), static_cast<input_value_t::code_t>(0x7ffd), 0x1234'5678);
constexpr auto canary_b
    = record(static_cast<input_value_t::type_t>(0x7ffc), static_cast<input_value_t::code_t>(0x7ffb), 0x2345'6789);
constexpr auto syn_marker = input_value_t::value_t{0x5eed};

static_assert(!std::copy_constructible<input_frame_t>);
static_assert(!std::move_constructible<input_frame_t>);

struct input_frame_test_t : Test
{
    static constexpr auto physical_capacity = std::size_t{10};

    template <std::size_t size>
    static auto matches(input_frame_t const& actual, std::array<input_value_t, size> const& expected) noexcept -> bool
    {
        if (actual.count() != size) return false;

        for (auto index = std::size_t{0}; index < size; ++index)
            if (actual.load(index) != expected[index]) return false;

        return true;
    }

    auto adapter(std::size_t capacity = physical_capacity) noexcept -> input_value_array_adapter_t
    {
        return input_value_array_adapter_t{storage_.data(), capacity};
    }

    std::array<input_value_t, physical_capacity> storage_{};
};

TEST_F(input_frame_test_t, accepts_framed_input)
{
    storage_[0] = syn_report();
    auto values = adapter();
    auto const sut = input_frame_t{values, 1};

    EXPECT_TRUE(sut.valid());
    EXPECT_EQ(sut.count(), 1u);
}

TEST_F(input_frame_test_t, zero_count_is_invalid)
{
    auto values = adapter();
    auto const sut = input_frame_t{values, 0};

    EXPECT_FALSE(sut.valid());
    EXPECT_EQ(sut.count(), 0u);
}

TEST_F(input_frame_test_t, count_exceeding_capacity_is_invalid)
{
    auto values = adapter(2);
    auto const sut = input_frame_t{values, 3};

    EXPECT_FALSE(sut.valid());
    EXPECT_EQ(sut.count(), 3u);
}

TEST_F(input_frame_test_t, missing_syn_report_is_invalid)
{
    storage_[0] = event_a;
    auto values = adapter();
    auto const sut = input_frame_t{values, 1};

    EXPECT_FALSE(sut.valid());
}

TEST_F(input_frame_test_t, final_syn_other_than_report_is_invalid)
{
    storage_[0] = record(input_value_t::type_t::syn, input_value_t::code_t{1}, 0);
    auto values = adapter();
    auto const sut = input_frame_t{values, 1};

    EXPECT_FALSE(sut.valid());
}

TEST_F(input_frame_test_t, load_reads_occupied_record)
{
    storage_[0] = event_a;
    storage_[1] = syn_report();
    auto values = adapter();
    auto const sut = input_frame_t{values, 2};

    EXPECT_EQ(sut.load(0), event_a);
    EXPECT_EQ(sut.load(1), syn_report());
}

TEST_F(input_frame_test_t, store_replaces_payload_record_without_changing_count)
{
    storage_[0] = event_a;
    storage_[1] = syn_report();
    auto values = adapter();
    auto sut = input_frame_t{values, 2};

    sut.store(0, event_b);

    EXPECT_EQ(sut.count(), 2u);
    EXPECT_TRUE(matches(sut, std::array{event_b, syn_report()}));
}

TEST_F(input_frame_test_t, appends_before_syn_report)
{
    storage_[0] = syn_report(syn_marker);
    auto values = adapter(2);
    auto sut = input_frame_t{values, 1};

    ASSERT_TRUE(sut.try_append(event_a));

    EXPECT_TRUE(matches(sut, std::array{event_a, syn_report(syn_marker)}));
}

TEST_F(input_frame_test_t, append_preserves_existing_record_order)
{
    storage_[0] = event_a;
    storage_[1] = event_b;
    storage_[2] = syn_report(syn_marker);
    auto values = adapter(4);
    auto sut = input_frame_t{values, 3};

    ASSERT_TRUE(sut.try_append(event_c));

    EXPECT_TRUE(matches(sut, std::array{event_a, event_b, event_c, syn_report(syn_marker)}));
}

TEST_F(input_frame_test_t, append_at_capacity_fails_without_mutation)
{
    storage_[0] = event_a;
    storage_[1] = syn_report(syn_marker);
    storage_[2] = canary_a;
    storage_[3] = canary_b;
    auto values = adapter(2);
    auto sut = input_frame_t{values, 2};

    EXPECT_FALSE(sut.try_append(event_b));

    EXPECT_TRUE(matches(sut, std::array{event_a, syn_report(syn_marker)}));
    EXPECT_EQ(storage_[2], canary_a);
    EXPECT_EQ(storage_[3], canary_b);
}

TEST_F(input_frame_test_t, append_does_not_write_beyond_capacity)
{
    storage_[0] = syn_report();
    storage_[2] = canary_a;
    storage_[3] = canary_b;
    auto values = adapter(2);
    auto sut = input_frame_t{values, 1};

    ASSERT_TRUE(sut.try_append(event_a));

    EXPECT_EQ(storage_[2], canary_a);
    EXPECT_EQ(storage_[3], canary_b);
}

TEST_F(input_frame_test_t, erases_first_payload_record)
{
    storage_[0] = event_a;
    storage_[1] = event_b;
    storage_[2] = event_c;
    storage_[3] = syn_report();
    auto values = adapter();
    auto sut = input_frame_t{values, 4};

    sut.erase(0);

    EXPECT_TRUE(matches(sut, std::array{event_b, event_c, syn_report()}));
}

TEST_F(input_frame_test_t, erases_middle_payload_record)
{
    storage_[0] = event_a;
    storage_[1] = event_b;
    storage_[2] = event_c;
    storage_[3] = syn_report();
    auto values = adapter();
    auto sut = input_frame_t{values, 4};

    sut.erase(1);

    EXPECT_TRUE(matches(sut, std::array{event_a, event_c, syn_report()}));
}

TEST_F(input_frame_test_t, erases_final_payload_record)
{
    storage_[0] = event_a;
    storage_[1] = event_b;
    storage_[2] = event_c;
    storage_[3] = syn_report();
    auto values = adapter();
    auto sut = input_frame_t{values, 4};

    sut.erase(2);

    EXPECT_TRUE(matches(sut, std::array{event_a, event_b, syn_report()}));
}

TEST_F(input_frame_test_t, erases_adjacent_pair)
{
    storage_[0] = event_a;
    storage_[1] = event_b;
    storage_[2] = event_c;
    storage_[3] = event_d;
    storage_[4] = syn_report();
    auto values = adapter();
    auto sut = input_frame_t{values, 5};

    sut.erase(1, 2);

    EXPECT_TRUE(matches(sut, std::array{event_a, event_d, syn_report()}));
}

TEST_F(input_frame_test_t, erases_nonadjacent_pair)
{
    storage_[0] = event_a;
    storage_[1] = event_b;
    storage_[2] = event_c;
    storage_[3] = event_d;
    storage_[4] = event_e;
    storage_[5] = event_f;
    storage_[6] = syn_report();
    auto values = adapter();
    auto sut = input_frame_t{values, 7};

    sut.erase(1, 4);

    EXPECT_TRUE(matches(sut, std::array{event_a, event_c, event_d, event_f, syn_report()}));
}

TEST_F(input_frame_test_t, pair_erase_accepts_descending_indices)
{
    storage_[0] = event_a;
    storage_[1] = event_b;
    storage_[2] = event_c;
    storage_[3] = event_d;
    storage_[4] = syn_report();
    auto values = adapter();
    auto sut = input_frame_t{values, 5};

    sut.erase(3, 1);

    EXPECT_TRUE(matches(sut, std::array{event_a, event_c, syn_report()}));
}

TEST_F(input_frame_test_t, erase_does_not_write_beyond_capacity)
{
    storage_[0] = event_a;
    storage_[1] = event_b;
    storage_[2] = syn_report();
    storage_[3] = canary_a;
    storage_[4] = canary_b;
    auto values = adapter(3);
    auto sut = input_frame_t{values, 3};

    sut.erase(0);

    EXPECT_EQ(storage_[3], canary_a);
    EXPECT_EQ(storage_[4], canary_b);
}

TEST_F(input_frame_test_t, pair_erase_does_not_write_beyond_capacity)
{
    storage_[0] = event_a;
    storage_[1] = event_b;
    storage_[2] = event_c;
    storage_[3] = syn_report();
    storage_[4] = canary_a;
    storage_[5] = canary_b;
    auto values = adapter(4);
    auto sut = input_frame_t{values, 4};

    sut.erase(0, 2);

    EXPECT_EQ(storage_[4], canary_a);
    EXPECT_EQ(storage_[5], canary_b);
}

TEST_F(input_frame_test_t, append_then_erase_preserves_report)
{
    storage_[0] = event_a;
    storage_[1] = syn_report(syn_marker);
    auto values = adapter(3);
    auto sut = input_frame_t{values, 2};

    ASSERT_TRUE(sut.try_append(event_b));
    sut.erase(1);

    EXPECT_TRUE(matches(sut, std::array{event_a, syn_report(syn_marker)}));
}

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST_F(input_frame_test_t, load_aborts_on_out_of_bounds_index)
{
    storage_[0] = syn_report();
    auto values = adapter(2);
    auto const sut = input_frame_t{values, 1};

    EXPECT_DEBUG_DEATH(static_cast<void>(sut.load(1)), "index < count_");
}

TEST_F(input_frame_test_t, store_aborts_on_syn_report_index)
{
    storage_[0] = event_a;
    storage_[1] = syn_report();
    auto values = adapter();
    auto sut = input_frame_t{values, 2};

    EXPECT_DEBUG_DEATH(sut.store(1, event_b), "index < count_ - 1");
}

TEST_F(input_frame_test_t, erase_aborts_on_syn_report_index)
{
    storage_[0] = event_a;
    storage_[1] = syn_report();
    auto values = adapter();
    auto sut = input_frame_t{values, 2};

    EXPECT_DEBUG_DEATH(sut.erase(1), "index < count_ - 1");
}

TEST_F(input_frame_test_t, erase_aborts_when_only_syn_report_remains)
{
    storage_[0] = syn_report();
    auto values = adapter();
    auto sut = input_frame_t{values, 1};

    EXPECT_DEBUG_DEATH(sut.erase(0), "count_ > 1");
}

TEST_F(input_frame_test_t, pair_erase_aborts_on_duplicate_index)
{
    storage_[0] = event_a;
    storage_[1] = event_b;
    storage_[2] = syn_report();
    auto values = adapter();
    auto sut = input_frame_t{values, 3};

    EXPECT_DEBUG_DEATH(sut.erase(0, 0), "first_index != second_index");
}

TEST_F(input_frame_test_t, mutation_aborts_on_invalid_frame)
{
    auto values = adapter();
    auto sut = input_frame_t{values, 0};

    EXPECT_DEBUG_DEATH(static_cast<void>(sut.try_append(event_a)), "valid_");
}

#endif

} // namespace
} // namespace crv::pipeline
