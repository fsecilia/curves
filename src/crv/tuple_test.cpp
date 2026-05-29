// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "tuple.hpp"
#include <crv/math/int_traits.hpp>
#include <crv/test/test.hpp>

namespace crv {
namespace {

struct a_t
{};

struct b_t
{};

struct c_t
{};

//
// transform_tuple_t
//

namespace tuple_transform_tests {

template <typename element_t> struct transform_t;

static_assert(std::same_as<tuple<>, transform_tuple_t<tuple<>, transform_t>>);
static_assert(std::same_as<tuple<transform_t<a_t>>, transform_tuple_t<tuple<a_t>, transform_t>>);
static_assert(std::same_as<tuple<transform_t<a_t>, transform_t<b_t>, transform_t<c_t>>,
    transform_tuple_t<tuple<a_t, b_t, c_t>, transform_t>>);

} // namespace tuple_transform_tests

//
// tuple_index_t
//

namespace tuple_index_tests {

static_assert(tuple_index_v<a_t, std::tuple<a_t>> == 0);
static_assert(tuple_index_v<void, std::tuple<a_t>> == 1);

static_assert(tuple_index_v<a_t, std::tuple<a_t, b_t>> == 0);
static_assert(tuple_index_v<b_t, std::tuple<a_t, b_t>> == 1);
static_assert(tuple_index_v<void, std::tuple<a_t, b_t>> == 2);

static_assert(tuple_index_v<a_t, std::tuple<a_t, b_t, c_t>> == 0);
static_assert(tuple_index_v<b_t, std::tuple<a_t, b_t, c_t>> == 1);
static_assert(tuple_index_v<c_t, std::tuple<a_t, b_t, c_t>> == 2);
static_assert(tuple_index_v<void, std::tuple<a_t, b_t, c_t>> == 3);

} // namespace tuple_index_tests

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

static_assert(tuple_for_each(std::tuple<>{}, counter_t{}).count == 0);
static_assert(tuple_for_each(std::tuple<int_t>{3}, counter_t{}).count == 3);
static_assert(tuple_for_each(std::tuple<int_t const, float_t>{3, 5.0}, counter_t{}).count == 8);
static_assert(tuple_for_each(std::tuple<int_t, float_t const&, a_t>{3, 5.0, a_t{}}, counter_t{}).count == 9);

static_assert(tuple_enumerate(std::tuple<>{}, counter_t{}).count == 0);
static_assert(tuple_enumerate(std::tuple<int_t>{3}, counter_t{}).count == 3);
static_assert(tuple_enumerate(std::tuple<int_t const, float_t>{3, 5.0}, counter_t{}).count == 9);
static_assert(tuple_enumerate(std::tuple<int_t, float_t const&, a_t>{3, 5.0, a_t{}}, counter_t{}).count == 12);

} // namespace iteration_tests

} // namespace
} // namespace crv
