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

//
// to_variant (index): result holds alternative at active_index
//

constexpr auto to_variant_idx_index(std::size_t which) -> std::size_t
{
    return to_variant<variant_t>(tuple_t{7, 3.5, 'k'}, which).index();
}
static_assert(to_variant_idx_index(0) == 0);
static_assert(to_variant_idx_index(1) == 1);
static_assert(to_variant_idx_index(2) == 2);

constexpr auto to_variant_idx0_value() -> int_t
{
    return std::get<0>(to_variant<variant_t>(tuple_t{7, 3.5, 'k'}, 0));
}
static_assert(to_variant_idx0_value() == 7);

constexpr auto to_variant_idx1_value() -> float_t
{
    return std::get<1>(to_variant<variant_t>(tuple_t{7, 3.5, 'k'}, 1));
}
static_assert(to_variant_idx1_value() == 3.5);

constexpr auto to_variant_idx2_value() -> char
{
    return std::get<2>(to_variant<variant_t>(tuple_t{7, 3.5, 'k'}, 2));
}
static_assert(to_variant_idx2_value() == 'k');

//
// to_variant (variant&): keeps active index, pulls value from corresponding tuple element
//

constexpr auto to_variant_ref_index() -> std::size_t
{
    auto dst = variant_t{3.5};
    to_variant(dst, tuple_t{7, 9.25, 'k'});
    return dst.index();
}
static_assert(to_variant_ref_index() == 1);

constexpr auto to_variant_ref_value() -> float_t
{
    auto dst = variant_t{3.5};
    to_variant(dst, tuple_t{7, 9.25, 'k'});
    return std::get<1>(dst);
}
static_assert(to_variant_ref_value() == 9.25);

//
// round trip: persistence cycle (variant -> tuple + index -> variant)
//

constexpr auto roundtrip_index() -> std::size_t
{
    auto original = variant_t{3.5};
    auto const original_index = original.index();
    auto tuple = tuple_t{};
    from_variant(tuple, std::move(original));
    return to_variant<variant_t>(std::move(tuple), original_index).index();
}
static_assert(roundtrip_index() == 1);

constexpr auto roundtrip_value() -> float_t
{
    auto original = variant_t{3.5};
    auto const original_index = original.index();
    auto tuple = tuple_t{};
    from_variant(tuple, std::move(original));
    return std::get<1>(to_variant<variant_t>(std::move(tuple), original_index));
}
static_assert(roundtrip_value() == 3.5);

} // namespace
} // namespace crv::variant
