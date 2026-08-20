// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "input_value_array_view.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <array>
#include <cstddef>

namespace crv {
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

constexpr auto record(input_value_t::type_t type, input_value_t::code_rel_t code, input_value_t::value_t value) noexcept
    -> input_value_t
{
    return record(type, static_cast<input_value_t::code_t>(code), value);
}

constexpr auto syn_report(input_value_t::value_t value = 0) noexcept -> input_value_t
{
    return record(input_value_t::type_t::syn, input_value_t::code_syn_t::report, value);
}

constexpr auto rel_x(input_value_t::value_t value) noexcept -> input_value_t
{
    return record(input_value_t::type_t::rel, input_value_t::code_rel_t::x, value);
}

constexpr auto rel_y(input_value_t::value_t value) noexcept -> input_value_t
{
    return record(input_value_t::type_t::rel, input_value_t::code_rel_t::y, value);
}

constexpr auto same(input_value_t const& lhs, input_value_t const& rhs) noexcept -> bool
{
    return lhs == rhs;
}

template <std::size_t size>
auto matches(input_value_array_view_t const& actual, std::array<input_value_t, size> const& expected) noexcept -> bool
{
    if (actual.count() != size) return false;

    for (auto index = std::size_t{0}; index < size; ++index)
    {
        if (!same(actual.load(static_cast<std::size_t>(index)), expected[index])) return false;
    }

    return true;
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

constexpr auto canary_a
    = record(static_cast<input_value_t::type_t>(0x7ffe), static_cast<input_value_t::code_t>(0x7ffd), 0x1234'5678);
constexpr auto canary_b
    = record(static_cast<input_value_t::type_t>(0x7ffc), static_cast<input_value_t::code_t>(0x7ffb), 0x2345'6789);
constexpr auto syn_marker = input_value_t::value_t{0x5eed};

struct input_value_array_view_test_t : Test
{
    static constexpr auto physical_capacity = std::size_t{10};

    auto make(std::size_t count, std::size_t capacity = physical_capacity) noexcept -> input_value_array_view_t
    {
        return input_value_array_view_t{storage_.data(), count, capacity};
    }

    std::array<input_value_t, physical_capacity> storage_{};
};

TEST_F(input_value_array_view_test_t, load_round_trips_min)
{
    storage_[0] = rel_x(min<input_value_t::value_t>());
    storage_[1] = syn_report();

    auto const sut = make(2);

    EXPECT_TRUE(same(sut.load(0), rel_x(min<input_value_t::value_t>())));
}

TEST_F(input_value_array_view_test_t, load_round_trips_max)
{
    storage_[0] = rel_y(max<input_value_t::value_t>());
    storage_[1] = syn_report();

    auto const sut = make(2);

    EXPECT_TRUE(same(sut.load(0), rel_y(max<input_value_t::value_t>())));
}

TEST_F(input_value_array_view_test_t, store_replaces_occupied_record)
{
    storage_[0] = rel_x(1);
    storage_[1] = syn_report();

    auto sut = make(2);
    sut.store(0, rel_y(-37));

    EXPECT_TRUE(matches(sut, std::array{rel_y(-37), syn_report()}));
}

TEST_F(input_value_array_view_test_t, store_does_not_change_count)
{
    storage_[0] = rel_x(1);
    storage_[1] = syn_report();

    auto sut = make(2);
    sut.store(0, rel_y(2));

    EXPECT_EQ(sut.count(), 2u);
}

TEST_F(input_value_array_view_test_t, appends_x_before_syn_report)
{
    storage_[0] = syn_report(syn_marker);

    auto sut = make(1, 2);
    EXPECT_TRUE(sut.try_append(input_value_t::code_rel_t::x, 17));

    EXPECT_TRUE(matches(sut, std::array{rel_x(17), syn_report(syn_marker)}));
}

TEST_F(input_value_array_view_test_t, appends_y_before_syn_report)
{
    storage_[0] = syn_report(syn_marker);

    auto sut = make(1, 2);
    EXPECT_TRUE(sut.try_append(input_value_t::code_rel_t::y, -17));

    EXPECT_TRUE(matches(sut, std::array{rel_y(-17), syn_report(syn_marker)}));
}

TEST_F(input_value_array_view_test_t, append_preserves_existing_record_order)
{
    storage_[0] = event_a;
    storage_[1] = event_b;
    storage_[2] = syn_report(syn_marker);

    auto sut = make(3, 4);
    EXPECT_TRUE(sut.try_append(input_value_t::code_rel_t::x, 17));

    EXPECT_TRUE(matches(sut, std::array{event_a, event_b, rel_x(17), syn_report(syn_marker)}));
}

TEST_F(input_value_array_view_test_t, append_does_not_write_beyond_capacity)
{
    storage_[0] = syn_report();
    storage_[2] = canary_a;
    storage_[3] = canary_b;

    auto sut = make(1, 2);
    EXPECT_TRUE(sut.try_append(input_value_t::code_rel_t::x, 17));

    EXPECT_TRUE(same(storage_[2], canary_a) && same(storage_[3], canary_b));
}

TEST_F(input_value_array_view_test_t, append_at_capacity_fails_without_mutation)
{
    storage_[0] = event_a;
    storage_[1] = syn_report(syn_marker);
    storage_[2] = canary_a;
    storage_[3] = canary_b;

    auto sut = make(2, 2);
    EXPECT_FALSE(sut.try_append(input_value_t::code_rel_t::x, 17));

    EXPECT_TRUE(matches(sut, std::array{event_a, syn_report(syn_marker)}));
    EXPECT_TRUE(same(storage_[2], canary_a) && same(storage_[3], canary_b));
}

TEST_F(input_value_array_view_test_t, erases_first_payload_record)
{
    storage_[0] = rel_x(3);
    storage_[1] = event_b;
    storage_[2] = event_c;
    storage_[3] = syn_report();

    auto sut = make(4);
    sut.erase(0);

    EXPECT_TRUE(matches(sut, std::array{event_b, event_c, syn_report()}));
}

TEST_F(input_value_array_view_test_t, erases_middle_payload_record)
{
    storage_[0] = event_a;
    storage_[1] = rel_y(5);
    storage_[2] = event_c;
    storage_[3] = syn_report();

    auto sut = make(4);
    sut.erase(1);

    EXPECT_TRUE(matches(sut, std::array{event_a, event_c, syn_report()}));
}

TEST_F(input_value_array_view_test_t, erases_final_payload_record)
{
    storage_[0] = event_a;
    storage_[1] = event_b;
    storage_[2] = rel_x(7);
    storage_[3] = syn_report();

    auto sut = make(4);
    sut.erase(2);

    EXPECT_TRUE(matches(sut, std::array{event_a, event_b, syn_report()}));
}

TEST_F(input_value_array_view_test_t, erases_adjacent_pair)
{
    storage_[0] = event_a;
    storage_[1] = rel_x(17);
    storage_[2] = rel_y(-29);
    storage_[3] = event_b;
    storage_[4] = syn_report();

    auto sut = make(5);
    sut.erase(1, 2);

    EXPECT_TRUE(matches(sut, std::array{event_a, event_b, syn_report()}));
}

TEST_F(input_value_array_view_test_t, erases_nonadjacent_pair)
{
    storage_[0] = event_a;
    storage_[1] = rel_x(17);
    storage_[2] = event_b;
    storage_[3] = event_c;
    storage_[4] = rel_y(-29);
    storage_[5] = event_d;
    storage_[6] = syn_report();

    auto sut = make(7);
    sut.erase(1, 4);

    EXPECT_TRUE(matches(sut, std::array{event_a, event_b, event_c, event_d, syn_report()}));
}

TEST_F(input_value_array_view_test_t, pair_erase_accepts_ascending_indexes)
{
    storage_[0] = event_a;
    storage_[1] = rel_x(17);
    storage_[2] = event_b;
    storage_[3] = rel_y(-29);
    storage_[4] = event_c;
    storage_[5] = syn_report();

    auto sut = make(6);
    sut.erase(1, 3);

    EXPECT_TRUE(matches(sut, std::array{event_a, event_b, event_c, syn_report()}));
}

TEST_F(input_value_array_view_test_t, pair_erase_accepts_descending_indices)
{
    storage_[0] = event_a;
    storage_[1] = rel_y(17);
    storage_[2] = event_b;
    storage_[3] = rel_x(-29);
    storage_[4] = event_c;
    storage_[5] = syn_report();

    auto sut = make(6);
    sut.erase(3, 1);

    EXPECT_TRUE(matches(sut, std::array{event_a, event_b, event_c, syn_report()}));
}

TEST_F(input_value_array_view_test_t, erase_does_not_write_beyond_capacity)
{
    storage_[0] = rel_x(2);
    storage_[1] = event_b;
    storage_[2] = syn_report();
    storage_[3] = canary_a;
    storage_[4] = canary_b;

    auto sut = make(3, 3);
    sut.erase(0);

    EXPECT_TRUE(same(storage_[3], canary_a) && same(storage_[4], canary_b));
}

TEST_F(input_value_array_view_test_t, pair_erase_does_not_write_beyond_capacity)
{
    storage_[0] = rel_x(2);
    storage_[1] = event_b;
    storage_[2] = rel_y(3);
    storage_[3] = syn_report();
    storage_[4] = canary_a;
    storage_[5] = canary_b;

    auto sut = make(4, 4);
    sut.erase(0, 2);

    EXPECT_TRUE(same(storage_[4], canary_a) && same(storage_[5], canary_b));
}

TEST_F(input_value_array_view_test_t, append_then_erase_preserves_report)
{
    storage_[0] = event_a;
    storage_[1] = syn_report(syn_marker);

    auto sut = make(2, 3);
    ASSERT_TRUE(sut.try_append(input_value_t::code_rel_t::x, 17));
    sut.erase(1);

    EXPECT_TRUE(matches(sut, std::array{event_a, syn_report(syn_marker)}));
}

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST_F(input_value_array_view_test_t, constructor_aborts_on_null_pointer)
{
    EXPECT_DEBUG_DEATH(input_value_array_view_t(nullptr, 1, 1), "nullptr != values_");
}

TEST_F(input_value_array_view_test_t, constructor_aborts_on_zero_capacity)
{
    EXPECT_DEBUG_DEATH(input_value_array_view_t(storage_.data(), 1, 0), "0 < capacity_");
}

TEST_F(input_value_array_view_test_t, constructor_aborts_on_zero_count)
{
    EXPECT_DEBUG_DEATH(input_value_array_view_t(storage_.data(), 0, 1), "0 < count_");
}

TEST_F(input_value_array_view_test_t, constructor_aborts_when_count_exceeds_capacity)
{
    EXPECT_DEBUG_DEATH(input_value_array_view_t(storage_.data(), 3, 2), "count_ <= capacity_");
}

TEST_F(input_value_array_view_test_t, constructor_aborts_when_missing_syn_report)
{
    storage_[0] = rel_x(10);

    EXPECT_DEBUG_DEATH(make(1), "type_t::syn");
}

TEST_F(input_value_array_view_test_t, constructor_aborts_when_final_syn_is_not_report)
{
    storage_[0] = record(input_value_t::type_t::syn, input_value_t::code_t{1}, 0);

    EXPECT_DEBUG_DEATH(make(1), "code_syn_t::report");
}

TEST_F(input_value_array_view_test_t, load_aborts_on_out_of_bounds_index)
{
    storage_[0] = syn_report();
    auto const sut = make(1);

    EXPECT_DEBUG_DEATH(static_cast<void>(sut.load(1)), "index < count_");
}

TEST_F(input_value_array_view_test_t, store_aborts_on_syn_report_index)
{
    storage_[0] = rel_x(10);
    storage_[1] = syn_report();
    auto sut = make(2);

    EXPECT_DEBUG_DEATH(sut.store(1, rel_y(20)), "index < count_ - 1");
}

TEST_F(input_value_array_view_test_t, store_aborts_on_out_of_bounds_index)
{
    storage_[0] = rel_x(10);
    storage_[1] = syn_report();
    auto sut = make(2);

    EXPECT_DEBUG_DEATH(sut.store(2, rel_y(20)), "index < count_ - 1");
}

TEST_F(input_value_array_view_test_t, erase_aborts_on_syn_report_index)
{
    storage_[0] = rel_x(10);
    storage_[1] = syn_report();
    auto sut = make(2);

    EXPECT_DEBUG_DEATH(sut.erase(1), "index < count_ - 1");
}

TEST_F(input_value_array_view_test_t, erase_aborts_on_out_of_bounds_index)
{
    storage_[0] = rel_x(10);
    storage_[1] = syn_report();
    auto sut = make(2);

    EXPECT_DEBUG_DEATH(sut.erase(2), "index < count_ - 1");
}

TEST_F(input_value_array_view_test_t, erase_aborts_when_only_syn_report_remains)
{
    storage_[0] = syn_report();
    auto sut = make(1);

    EXPECT_DEBUG_DEATH(sut.erase(0), "count_ > 1");
}

TEST_F(input_value_array_view_test_t, erase_aborts_when_record_is_not_rel)
{
    storage_[0] = event_a; // 0x01
    storage_[1] = syn_report();
    auto sut = make(2);

    EXPECT_DEBUG_DEATH(sut.erase(0), "type_t::rel");
}

TEST_F(input_value_array_view_test_t, erase_aborts_when_rel_code_is_not_x_or_y)
{
    storage_[0] = record(input_value_t::type_t::rel, input_value_t::code_t{0x08}, 10);
    storage_[1] = syn_report();
    auto sut = make(2);

    EXPECT_DEBUG_DEATH(sut.erase(0), "code_rel_t::y");
}

TEST_F(input_value_array_view_test_t, pair_erase_aborts_on_syn_report_index)
{
    storage_[0] = rel_x(10);
    storage_[1] = rel_y(20);
    storage_[2] = syn_report();
    auto sut = make(3);

    EXPECT_DEBUG_DEATH(sut.erase(0, 2), "y_index < count_ - 1");
}

TEST_F(input_value_array_view_test_t, pair_erase_aborts_on_out_of_bounds_x_index)
{
    storage_[0] = rel_x(10);
    storage_[1] = rel_y(20);
    storage_[2] = syn_report();
    auto sut = make(3);

    EXPECT_DEBUG_DEATH(sut.erase(3, 1), "x_index < count_ - 1");
}

TEST_F(input_value_array_view_test_t, pair_erase_aborts_on_duplicate_index)
{
    storage_[0] = rel_x(10);
    storage_[1] = rel_y(20);
    storage_[2] = syn_report();
    auto sut = make(3);

    EXPECT_DEBUG_DEATH(sut.erase(0, 0), "x_index != y_index");
}

TEST_F(input_value_array_view_test_t, pair_erase_aborts_when_x_index_is_not_x_code)
{
    storage_[0] = rel_y(10);
    storage_[1] = rel_y(20);
    storage_[2] = syn_report();
    auto sut = make(3);

    EXPECT_DEBUG_DEATH(sut.erase(0, 1), "code_rel_t::x");
}

TEST_F(input_value_array_view_test_t, pair_erase_aborts_when_y_index_is_not_y_code)
{
    storage_[0] = rel_x(10);
    storage_[1] = rel_x(20);
    storage_[2] = syn_report();
    auto sut = make(3);

    EXPECT_DEBUG_DEATH(sut.erase(0, 1), "code_rel_t::y");
}

#endif

} // namespace
} // namespace crv
