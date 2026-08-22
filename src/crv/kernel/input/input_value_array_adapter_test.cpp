// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "input_value_array_adapter.hpp"
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

constexpr auto rel_x(input_value_t::value_t value) noexcept -> input_value_t
{
    return record(input_value_t::type_t::rel, static_cast<input_value_t::code_t>(input_value_t::code_rel_t::x), value);
}

constexpr auto rel_y(input_value_t::value_t value) noexcept -> input_value_t
{
    return record(input_value_t::type_t::rel, static_cast<input_value_t::code_t>(input_value_t::code_rel_t::y), value);
}

constexpr auto event_a
    = record(static_cast<input_value_t::type_t>(0x01), static_cast<input_value_t::code_t>(0x0100), 11);
constexpr auto event_b
    = record(static_cast<input_value_t::type_t>(0x04), static_cast<input_value_t::code_t>(0x0200), 22);

struct input_value_array_adapter_test_t : Test
{
    static constexpr auto capacity = std::size_t{4};

    auto make(std::size_t logical_capacity = capacity) noexcept -> input_value_array_adapter_t
    {
        return input_value_array_adapter_t{storage_.data(), logical_capacity};
    }

    std::array<input_value_t, capacity> storage_{};
};

TEST_F(input_value_array_adapter_test_t, reports_capacity)
{
    auto const sut = make(3);
    EXPECT_EQ(sut.capacity(), 3u);
}

TEST_F(input_value_array_adapter_test_t, accepts_zero_capacity)
{
    auto const sut = make(0);
    EXPECT_EQ(sut.capacity(), 0u);
}

TEST_F(input_value_array_adapter_test_t, load_round_trips_min)
{
    storage_[0] = rel_x(min<input_value_t::value_t>());
    auto const sut = make();

    EXPECT_EQ(sut.load(0), rel_x(min<input_value_t::value_t>()));
}

TEST_F(input_value_array_adapter_test_t, load_round_trips_max)
{
    storage_[0] = rel_y(max<input_value_t::value_t>());
    auto const sut = make();

    EXPECT_EQ(sut.load(0), rel_y(max<input_value_t::value_t>()));
}

TEST_F(input_value_array_adapter_test_t, store_replaces_record)
{
    storage_[0] = rel_x(1);
    auto sut = make();

    sut.store(0, rel_y(-37));

    EXPECT_EQ(storage_[0], rel_y(-37));
}

TEST_F(input_value_array_adapter_test_t, move_handles_overlapping_records_to_higher_indices)
{
    storage_[0] = event_a;
    storage_[1] = event_b;
    auto sut = make();

    sut.move(1, 0, 2);

    EXPECT_EQ(storage_[1], event_a);
    EXPECT_EQ(storage_[2], event_b);
}

TEST_F(input_value_array_adapter_test_t, move_handles_overlapping_records_to_lower_indices)
{
    storage_[1] = event_a;
    storage_[2] = event_b;
    auto sut = make();

    sut.move(0, 1, 2);

    EXPECT_EQ(storage_[0], event_a);
    EXPECT_EQ(storage_[1], event_b);
}

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST_F(input_value_array_adapter_test_t, constructor_aborts_on_null_pointer)
{
    EXPECT_DEBUG_DEATH(input_value_array_adapter_t(nullptr, 1), "nullptr != values_");
}

TEST_F(input_value_array_adapter_test_t, load_aborts_on_out_of_bounds_index)
{
    auto const sut = make(2);
    EXPECT_DEBUG_DEATH(static_cast<void>(sut.load(2)), "index < capacity_");
}

TEST_F(input_value_array_adapter_test_t, store_aborts_on_out_of_bounds_index)
{
    auto sut = make(2);
    EXPECT_DEBUG_DEATH(sut.store(2, event_a), "index < capacity_");
}

TEST_F(input_value_array_adapter_test_t, move_aborts_when_source_range_exceeds_capacity)
{
    auto sut = make(2);
    EXPECT_DEBUG_DEATH(sut.move(0, 1, 2), "count <= capacity_ - source");
}

TEST_F(input_value_array_adapter_test_t, move_aborts_when_destination_range_exceeds_capacity)
{
    auto sut = make(2);
    EXPECT_DEBUG_DEATH(sut.move(1, 0, 2), "count <= capacity_ - destination");
}

#endif

} // namespace
} // namespace crv
