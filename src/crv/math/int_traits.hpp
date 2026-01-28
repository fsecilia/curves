// SPDX-License-Identifier: MIT
/*!
  \file
  \brief 128-bit integer types and related concepts

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <cassert>
#include <type_traits>

namespace crv {

// --------------------------------------------------------------------------------------------------------------------
// integral
// --------------------------------------------------------------------------------------------------------------------

/*
    gcc's implementation of std::is_integral is not specialized for 128-bit types. This defers to std::is_integral_v,
    extending it to include 128-bit types.
*/
template <typename value_t>
struct is_integral
    : std::bool_constant<std::is_integral_v<value_t> || std::is_same_v<std::remove_cv_t<value_t>, int128_t>
                         || std::is_same_v<std::remove_cv_t<value_t>, uint128_t>>
{};

template <typename value_t> constexpr auto is_integral_v = is_integral<value_t>::value;

template <typename value_t>
concept integral = is_integral_v<value_t>;

// --------------------------------------------------------------------------------------------------------------------
// arithmetic
// --------------------------------------------------------------------------------------------------------------------

/*
    gcc's implementation of std::is_arithmetic is not specialized for 128-bit types. This defers to
    std::is_arithmetic_v, extending it to include 128-bit types.
*/
template <typename value_t>
struct is_arithmetic : std::bool_constant<is_integral_v<value_t> || std::is_floating_point_v<value_t>>
{};

template <typename value_t> constexpr auto is_arithmetic_v = is_arithmetic<value_t>::value;

template <typename value_t>
concept arithmetic = is_arithmetic_v<value_t>;

// --------------------------------------------------------------------------------------------------------------------
// signed
// --------------------------------------------------------------------------------------------------------------------

/*
    The standard family of signed traits uses std:is_arithmetic and std::is_integral, which exclude 128-bit types.
*/
template <typename value_t>
constexpr auto is_signed_v = [] {
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
// sized_integer_t
// --------------------------------------------------------------------------------------------------------------------

namespace detail::integer {

template <int_t size, bool is_signed> struct sized_integer_f;

// clang-format off
template <> struct sized_integer_f<1, false> { using type = uint8_t; };
template <> struct sized_integer_f<2, false> { using type = uint16_t; };
template <> struct sized_integer_f<4, false> { using type = uint32_t; };
template <> struct sized_integer_f<8, false> { using type = uint64_t; };
template <> struct sized_integer_f<16, false> { using type = uint128_t; };

template <> struct sized_integer_f<1, true> { using type = int8_t; };
template <> struct sized_integer_f<2, true> { using type = int16_t; };
template <> struct sized_integer_f<4, true> { using type = int32_t; };
template <> struct sized_integer_f<8, true> { using type = int64_t; };
template <> struct sized_integer_f<16, true> { using type = int128_t; };
// clang-format on

} // namespace detail::integer

template <int_t size, bool is_signed> using sized_integer_t = detail::integer::sized_integer_f<size, is_signed>::type;

} // namespace crv
