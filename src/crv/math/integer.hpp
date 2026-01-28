// SPDX-License-Identifier: MIT
/*!
  \file
  \brief safe integer casts

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <cassert>
#include <concepts>
#include <type_traits>
#include <utility>

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

// --------------------------------------------------------------------------------------------------------------------
// int_cast
// --------------------------------------------------------------------------------------------------------------------

//! asserts that from is in the representable range of to_t
template <std::integral to_t, std::integral from_t> constexpr auto int_cast(from_t from) noexcept -> to_t
{
    assert(std::in_range<to_t>(from) && "out of range integer cast");
    return static_cast<to_t>(from);
}

} // namespace crv
