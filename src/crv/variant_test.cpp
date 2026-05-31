// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "variant.hpp"
#include <crv/test/test.hpp>

namespace crv::variant {
namespace {

using tuple_t = std::tuple<int_t, float_t, char>;
using variant_t = std::variant<int_t, float_t, char>;

constexpr auto known_tuple() -> tuple_t
{
    return tuple_t{7, 3.5, 'k'};
}

template <std::size_t index> constexpr auto known_value() -> std::decay_t<std::tuple_element_t<index, tuple_t>>
{
    return std::get<index>(known_tuple());
}

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

template <std::size_t index> constexpr auto from_variant_lands() -> bool
{
    auto dst = tuple_t{-1, -1.0, '?'};
    from_variant(dst, variant_t{std::in_place_index<index>, known_value<index>()});
    return std::get<index>(dst) == known_value<index>();
}

static_assert(from_variant_lands<0>());
static_assert(from_variant_lands<1>());
static_assert(from_variant_lands<2>());

//
// from_variant: inactive tuple elements are left untouched
//

constexpr auto from_variant_leaves_others_untouched() -> bool
{
    auto dst = tuple_t{-1, -1.0, '?'};
    from_variant(dst, variant_t{3.5});
    return std::get<0>(dst) == -1 && std::get<2>(dst) == '?';
}
static_assert(from_variant_leaves_others_untouched());

//
// to_variant (index): result holds the alternative at active_index
//

template <std::size_t index> constexpr auto to_variant_holds() -> bool
{
    auto const dst = to_variant<variant_t>(known_tuple(), index);
    return dst.index() == index && std::get<index>(dst) == known_value<index>();
}

static_assert(to_variant_holds<0>());
static_assert(to_variant_holds<1>());
static_assert(to_variant_holds<2>());

//
// to_variant (variant&): keeps active index, pulls value from corresponding element
//

constexpr auto to_variant_ref_pulls_from_tuple() -> bool
{
    auto dst = variant_t{3.5};
    to_variant(dst, tuple_t{7, 9.25, 'k'});
    return dst.index() == 1 && std::get<1>(dst) == 9.25;
}
static_assert(to_variant_ref_pulls_from_tuple());

//
// round trip
//

template <std::size_t index> constexpr auto roundtrip_preserves() -> bool
{
    auto original = variant_t{std::in_place_index<index>, known_value<index>()};
    auto const original_index = original.index();

    auto tuple = tuple_t{};
    from_variant(tuple, std::move(original));

    auto const restored = to_variant<variant_t>(std::move(tuple), original_index);
    return restored.index() == index && std::get<index>(restored) == known_value<index>();
}

static_assert(roundtrip_preserves<0>());
static_assert(roundtrip_preserves<1>());
static_assert(roundtrip_preserves<2>());

//
// move-only types
//

struct move_only_t
{
    int_t value{};

    constexpr explicit move_only_t(int_t v) : value{v} {}

    constexpr move_only_t(move_only_t const&) = delete;
    constexpr auto operator=(move_only_t const&) -> move_only_t& = delete;

    constexpr move_only_t(move_only_t&&) = default;
    constexpr auto operator=(move_only_t&&) -> move_only_t& = default;
};

using move_only_tuple_t = std::tuple<move_only_t, float_t>;
using move_only_variant_t = std::variant<move_only_t, float_t>;

constexpr auto move_only_from_variant() -> int_t
{
    auto dst = move_only_tuple_t{move_only_t{-1}, -1.0};
    from_variant(dst, move_only_variant_t{move_only_t{37}});
    return std::get<0>(dst).value;
}
static_assert(move_only_from_variant() == 37);

constexpr auto move_only_to_variant() -> int_t
{
    auto src = move_only_tuple_t{move_only_t{37}, -1.0};
    auto dst = to_variant<move_only_variant_t>(std::move(src), 0);
    return std::get<0>(dst).value;
}
static_assert(move_only_to_variant() == 37);

//
// lvalue vs rvalue deduction
//

constexpr auto to_variant_lvalue_tuple() -> float_t
{
    auto src_tuple = tuple_t{7, 3.5, 'k'};
    auto dst_variant = variant_t{0.0};
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
// duplicate types
//

using duplicate_tuple_t = std::tuple<int_t, int_t>;
using duplicate_variant_t = std::variant<int_t, int_t>;

constexpr auto duplicate_types_to_variant() -> bool
{
    auto src = duplicate_tuple_t{10, 20};
    auto dst = to_variant<duplicate_variant_t>(std::move(src), 1);
    return dst.index() == 1 && std::get<1>(dst) == 20;
}
static_assert(duplicate_types_to_variant());

constexpr auto duplicate_types_from_variant() -> int_t
{
    auto dst = duplicate_tuple_t{-1, -1};

    // explicitly construct alternative at the second index
    auto src = duplicate_variant_t{std::in_place_index<1>, 37};

    from_variant(dst, std::move(src));
    return std::get<1>(dst);
}
static_assert(duplicate_types_from_variant() == 37);

//
// valueless_by_exception handling
//

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST(from_variant_death_test, valueless_by_exception_asserts)
{
    struct throw_on_copy_t
    {
        throw_on_copy_t() = default;
        [[noreturn]] throw_on_copy_t(throw_on_copy_t const&) { throw std::runtime_error("copy failed"); }
        [[noreturn]] auto operator=(throw_on_copy_t const&) -> throw_on_copy_t&
        {
            throw std::runtime_error("copy failed");
        }
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
