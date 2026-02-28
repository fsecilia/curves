// SPDX-License-Identifier: MIT
/*!
    \file
    \brief integer traits that include 128-bit types

    This module contains traits for integers. Much of it is dedicated to 128-bit types that are not included in the
    equivalent standard library traits.

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <crv/traits.hpp>
#include <algorithm>
#include <cassert>
#include <type_traits>

namespace crv {

// --------------------------------------------------------------------------------------------------------------------
// integral
// --------------------------------------------------------------------------------------------------------------------

template <typename value_t>
struct is_integral
    : std::bool_constant<std::is_integral_v<value_t> || std::is_same_v<std::remove_cv_t<value_t>, int128_t>
                         || std::is_same_v<std::remove_cv_t<value_t>, uint128_t>>
{};

template <typename value_t> inline constexpr auto is_integral_v = is_integral<value_t>::value;

template <typename value_t>
concept integral = is_integral_v<value_t>;

// --------------------------------------------------------------------------------------------------------------------
// arithmetic
// --------------------------------------------------------------------------------------------------------------------

template <typename value_t>
struct is_arithmetic : std::bool_constant<is_integral_v<value_t> || std::is_floating_point_v<value_t>>
{};

template <typename value_t> inline constexpr auto is_arithmetic_v = is_arithmetic<value_t>::value;

template <typename value_t>
concept arithmetic = is_arithmetic_v<value_t>;

// --------------------------------------------------------------------------------------------------------------------
// signed
// --------------------------------------------------------------------------------------------------------------------

template <typename value_t>
inline constexpr auto is_signed_v = [] {
    if constexpr (is_arithmetic_v<value_t>) { return value_t(-1) < value_t{0}; }
    else return false;
}();

template <typename value_t> struct is_signed : std::bool_constant<is_signed_v<value_t>>
{};

template <typename value_t>
concept signed_integral = integral<value_t> && is_signed_v<value_t>;

template <typename value_t>
concept unsigned_integral = integral<value_t> && !signed_integral<value_t>;

// --------------------------------------------------------------------------------------------------------------------
// make_unsigned
// --------------------------------------------------------------------------------------------------------------------

namespace detail {

template <typename src_t, int size> struct make_unsigned_f : std::make_unsigned<src_t>
{};

template <typename src_t> struct make_unsigned_f<src_t, sizeof(uint128_t)>
{
    using type = copy_cv_t<uint128_t, src_t>;
};

} // namespace detail

template <typename src_t> struct make_unsigned : detail::make_unsigned_f<src_t, sizeof(src_t)>
{};

template <typename src_t> using make_unsigned_t = make_unsigned<src_t>::type;

// --------------------------------------------------------------------------------------------------------------------
// sized_integer_t
// --------------------------------------------------------------------------------------------------------------------

namespace detail::integer {

template <int_t size, bool is_signed> struct sized_integer_t;

// clang-format off
template <> struct sized_integer_t<1, false> { using type = uint8_t; };
template <> struct sized_integer_t<2, false> { using type = uint16_t; };
template <> struct sized_integer_t<4, false> { using type = uint32_t; };
template <> struct sized_integer_t<8, false> { using type = uint64_t; };
template <> struct sized_integer_t<16, false> { using type = uint128_t; };

template <> struct sized_integer_t<1, true> { using type = int8_t; };
template <> struct sized_integer_t<2, true> { using type = int16_t; };
template <> struct sized_integer_t<4, true> { using type = int32_t; };
template <> struct sized_integer_t<8, true> { using type = int64_t; };
template <> struct sized_integer_t<16, true> { using type = int128_t; };
// clang-format on

} // namespace detail::integer

template <int size, bool is_signed> using sized_integer_t = detail::integer::sized_integer_t<size, is_signed>::type;

// --------------------------------------------------------------------------------------------------------------------
// Integer Promotions
// --------------------------------------------------------------------------------------------------------------------

/// integer type sized to the larger of lhs_t and rhs_t and signed if either is signed
template <integral lhs_t, integral rhs_t>
using promoted_t = sized_integer_t<std::max(sizeof(lhs_t), sizeof(rhs_t)), is_signed_v<lhs_t> || is_signed_v<rhs_t>>;

template <integral value_t> using wider_t = sized_integer_t<sizeof(value_t) * 2, is_signed_v<value_t>>;

} // namespace crv
