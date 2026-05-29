// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "variant.hpp"
#include <crv/test/test.hpp>

namespace crv::variant {
namespace {

using tuple_t = std::tuple<int_t, float_t, char>;
using variant_t = std::variant<int_t, float_t, char>;

//
// has_same_types
//

static_assert(has_same_types<std::tuple<int_t, float_t>, std::variant<int_t, float_t>>);
static_assert(!has_same_types<std::tuple<int_t, float_t>, std::variant<float_t, int>>);
static_assert(!has_same_types<std::tuple<int_t, float_t>, std::variant<int_t, char>>);
static_assert(!has_same_types<std::tuple<int_t>, std::variant<int_t, float_t>>);
static_assert(!has_same_types<std::tuple<int_t, float_t>, std::tuple<int_t, float_t>>);

//
// from_variant_t / to_variant_t
//

static_assert(std::is_same_v<from_variant_t<variant_t>, tuple_t>);
static_assert(std::is_same_v<to_variant_t<tuple_t>, variant_t>);

//
// from_variant: held alternative lands in matching tuple element
//

constexpr auto from_variant_int() -> int_t
{
    auto dst = tuple_t{-1, -1.0, '?'};
    from_variant(dst, variant_t{7});
    return std::get<0>(dst);
}
static_assert(from_variant_int() == 7);

constexpr auto from_variant_double() -> float_t
{
    auto dst = tuple_t{-1, -1.0, '?'};
    from_variant(dst, variant_t{3.5});
    return std::get<1>(dst);
}
static_assert(from_variant_double() == 3.5);

constexpr auto from_variant_char() -> char
{
    auto dst = tuple_t{-1, -1.0, '?'};
    from_variant(dst, variant_t{'k'});
    return std::get<2>(dst);
}
static_assert(from_variant_char() == 'k');

// inactive tuple elements are left untouched
constexpr auto from_variant_leaves_int() -> int_t
{
    auto dst = tuple_t{-1, -1.0, '?'};
    from_variant(dst, variant_t{3.5});
    return std::get<0>(dst);
}
static_assert(from_variant_leaves_int() == -1);

constexpr auto from_variant_leaves_char() -> char
{
    auto dst = tuple_t{-1, -1.0, '?'};
    from_variant(dst, variant_t{3.5});
    return std::get<2>(dst);
}
static_assert(from_variant_leaves_char() == '?');

} // namespace
} // namespace crv::variant
