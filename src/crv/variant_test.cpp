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
static_assert(has_same_types<std::tuple<int_t, float_t>, std::variant<int_t, float_t>&>);
static_assert(has_same_types<std::tuple<int_t, float_t>&, std::variant<int_t, float_t>>);
static_assert(has_same_types<std::tuple<int_t, float_t>&, std::variant<int_t, float_t>&>);
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

constexpr auto to_variant_index(std::size_t which) -> std::size_t
{
    return to_variant<variant_t>(tuple_t{7, 3.5, 'k'}, which).index();
}
static_assert(to_variant_index(0) == 0);
static_assert(to_variant_index(1) == 1);
static_assert(to_variant_index(2) == 2);

constexpr auto to_variant_index_0() -> int_t
{
    return std::get<0>(to_variant<variant_t>(tuple_t{7, 3.5, 'k'}, 0));
}
static_assert(to_variant_index_0() == 7);

constexpr auto to_variant_index_1() -> float_t
{
    return std::get<1>(to_variant<variant_t>(tuple_t{7, 3.5, 'k'}, 1));
}
static_assert(to_variant_index_1() == 3.5);

constexpr auto to_variant_index_2() -> char
{
    return std::get<2>(to_variant<variant_t>(tuple_t{7, 3.5, 'k'}, 2));
}
static_assert(to_variant_index_2() == 'k');

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
// round trip
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

//
// lvalue vs rvalue deduction
//

constexpr auto to_variant_lvalue_tuple() -> float_t
{
    auto src_tuple = tuple_t{7, 3.5, 'k'};
    auto dst_variant = variant_t{0.0f};
    to_variant(dst_variant, src_tuple);
    return std::get<1>(dst_variant);
}
static_assert(to_variant_lvalue_tuple() == 3.5);

constexpr auto from_variant_lvalue_variant() -> char
{
    auto dst_tuple = tuple_t{-1, -1.0, '?'};
    auto src_variant = variant_t{'k'};
    from_variant(dst_tuple, src_variant);
    return std::get<2>(dst_tuple);
}
static_assert(from_variant_lvalue_variant() == 'k');

//
// valueless_by_exception handling
//

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST(from_variant_death_test, valueless_by_exception_asserts)
{
    struct throw_on_copy_t
    {
        throw_on_copy_t() = default;
        throw_on_copy_t(throw_on_copy_t const&) { throw std::runtime_error("copy failed"); }
        auto operator=(throw_on_copy_t const&) -> throw_on_copy_t& { throw std::runtime_error("copy failed"); }
        throw_on_copy_t(throw_on_copy_t&&) = default;
        auto operator=(throw_on_copy_t&&) -> throw_on_copy_t& = default;
    };

    using throwing_tuple_t = std::tuple<int_t, throw_on_copy_t>;
    using throwing_variant_t = std::variant<int_t, throw_on_copy_t>;

    // force variant into valueless-by-exception state
    auto bad_variant = throwing_variant_t{5};
    try
    {
        auto thrower = throw_on_copy_t{};
        bad_variant.emplace<1>(thrower); // throws, putting bad_variant into valueless state
    }
    catch (...)
    {}

    auto dst = throwing_tuple_t{};
    EXPECT_DEBUG_DEATH(from_variant(dst, std::move(bad_variant)), "valueless_by_exception");
}

#endif

} // namespace
} // namespace crv::variant
