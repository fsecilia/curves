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

} // namespace
} // namespace crv
