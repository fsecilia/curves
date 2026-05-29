// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "tuple.hpp"
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

} // namespace
} // namespace crv
