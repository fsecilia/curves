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
#include <utility>

namespace crv {

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

//! asserts that from is in the representable range of to_t
template <std::integral to_t, std::integral from_t> constexpr auto int_cast(from_t from) noexcept -> to_t
{
    assert(std::in_range<to_t>(from) && "out of range integer cast");
    return static_cast<to_t>(from);
}

} // namespace crv
