// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "tuple.hpp"
#include <crv/math/int_traits.hpp>
#include <crv/test/test.hpp>

namespace crv::tuple {
namespace {

struct a_t
{};

struct b_t
{};

struct c_t
{};

//
// index_t
//

namespace index_tests {

static_assert(index_v<a_t, std::tuple<a_t>> == 0);
static_assert(index_v<void, std::tuple<a_t>> == 1);

static_assert(index_v<a_t, std::tuple<a_t, b_t>> == 0);
static_assert(index_v<b_t, std::tuple<a_t, b_t>> == 1);
static_assert(index_v<void, std::tuple<a_t, b_t>> == 2);

static_assert(index_v<a_t, std::tuple<a_t, b_t, c_t>> == 0);
static_assert(index_v<b_t, std::tuple<a_t, b_t, c_t>> == 1);
static_assert(index_v<c_t, std::tuple<a_t, b_t, c_t>> == 2);
static_assert(index_v<void, std::tuple<a_t, b_t, c_t>> == 3);

} // namespace index_tests

//
// transform_t
//

namespace transform_tests {

template <typename element_t> struct op_t;

static_assert(std::same_as<std::tuple<>, transform_t<std::tuple<>, op_t>>);
static_assert(std::same_as<std::tuple<op_t<a_t>>, transform_t<std::tuple<a_t>, op_t>>);
static_assert(std::same_as<std::tuple<op_t<a_t>, op_t<b_t>, op_t<c_t>>, transform_t<std::tuple<a_t, b_t, c_t>, op_t>>);

} // namespace transform_tests

//
// iteration
//

namespace iteration_tests {

struct counter_t
{
    int_t count = 0;

    constexpr auto operator()(arithmetic auto&& element) noexcept -> void { count += static_cast<int_t>(element); }
    constexpr auto operator()(a_t const&) noexcept -> void { ++count; }

    constexpr auto operator()(std::size_t index, arithmetic auto&& element) noexcept -> void
    {
        count += static_cast<int_t>(index) + static_cast<int_t>(element);
    }

    constexpr auto operator()(std::size_t index, a_t const&) noexcept -> void
    {
        count += static_cast<int_t>(index) + 1;
    }
};

static_assert(for_each(std::tuple<>{}, counter_t{}).count == 0);
static_assert(for_each(std::tuple<int_t>{3}, counter_t{}).count == 3);
static_assert(for_each(std::tuple<int_t const, float_t>{3, 5.0}, counter_t{}).count == 8);
static_assert(for_each(std::tuple<int_t, float_t const&, a_t>{3, 5.0, a_t{}}, counter_t{}).count == 9);

static_assert(enumerate(std::tuple<>{}, counter_t{}).count == 0);
static_assert(enumerate(std::tuple<int_t>{3}, counter_t{}).count == 3);
static_assert(enumerate(std::tuple<int_t const, float_t>{3, 5.0}, counter_t{}).count == 9);
static_assert(enumerate(std::tuple<int_t, float_t const&, a_t>{3, 5.0, a_t{}}, counter_t{}).count == 12);

} // namespace iteration_tests

namespace visit_at_tests {

// tests selecting by index
constexpr auto test_value_selection(std::size_t index) -> int_t
{
    auto const tuple = std::tuple<int_t, float_t, a_t>{37, 5.0f, a_t{}};
    auto result = int_t{0};

    visit_at(tuple, index, [&]<typename type_t>(type_t const& element) {
        if constexpr (std::same_as<type_t, int_t>) result = element;
        else if constexpr (std::same_as<type_t, float_t>) result = static_cast<int_t>(element) * 10;
        else if constexpr (std::same_as<type_t, a_t>) result = 99;
    });

    return result;
}
static_assert(test_value_selection(0) == 37);
static_assert(test_value_selection(1) == 50);
static_assert(test_value_selection(2) == 99);

// tests that rvalue-refness is forwarded
constexpr auto test_perfect_forwarding() -> bool
{
    auto tuple = std::tuple<int_t>{37};
    auto is_rvalue_reference = false;

    visit_at(
        std::move(tuple), 0, [&](auto&& element) { is_rvalue_reference = std::same_as<decltype(element), int_t&&>; });

    return is_rvalue_reference;
}
static_assert(test_perfect_forwarding());

// tests that lvalue-refness is forwarded
constexpr auto test_lvalue_forwarding() -> bool
{
    auto tuple = std::tuple<int_t>{37};
    auto is_lvalue_reference = false;

    visit_at(tuple, 0, [&](auto&& element) { is_lvalue_reference = std::same_as<decltype(element), int_t&>; });

    return is_lvalue_reference;
}
static_assert(test_lvalue_forwarding());

} // namespace visit_at_tests

} // namespace
} // namespace crv::tuple
