// SPDX-License-Identifier: MIT

/// \file
/// \brief integer fundamentals
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/limits.hpp>
#include <bit>
#include <cassert>
#include <climits>

namespace crv {

// --------------------------------------------------------------------------------------------------------------------
// log2
// --------------------------------------------------------------------------------------------------------------------

template <integral value_t> constexpr auto log2(value_t value) noexcept -> value_t
{
    assert(value > 0 && "log2: domain error");
    return static_cast<value_t>(std::bit_width(value) - 1);
}

// --------------------------------------------------------------------------------------------------------------------
// conversions
// --------------------------------------------------------------------------------------------------------------------

/// extends std::in_range to support 128-bit types
template <integral to_t, integral from_t> constexpr auto in_range(from_t from) noexcept -> bool
{
    if constexpr (is_signed_v<from_t> == is_signed_v<to_t>) { return min<to_t>() <= from && from <= max<to_t>(); }
    else if constexpr (is_signed_v<from_t>)
    {
        // signed->unsigned
        return 0 <= from && static_cast<make_unsigned_t<from_t>>(from) <= max<to_t>();
    }
    else
    {
        // unsigned->signed
        return from <= static_cast<make_unsigned_t<to_t>>(max<to_t>());
    }
}

/// asserts that from is in the representable range of to_t
template <integral to_t, integral from_t> constexpr auto int_cast(from_t from) noexcept -> to_t
{
    assert(in_range<to_t>(from) && "int_cast: input out of range");
    return static_cast<to_t>(from);
}

// --------------------------------------------------------------------------------------------------------------------
// types by size
// --------------------------------------------------------------------------------------------------------------------

namespace detail::integer {

template <int_t byte_count, bool is_signed> struct int_by_bytes_t;

// clang-format off
template <> struct int_by_bytes_t<1, false> { using type = uint8_t; };
template <> struct int_by_bytes_t<2, false> { using type = uint16_t; };
template <> struct int_by_bytes_t<4, false> { using type = uint32_t; };
template <> struct int_by_bytes_t<8, false> { using type = uint64_t; };
template <> struct int_by_bytes_t<16, false> { using type = uint128_t; };

template <> struct int_by_bytes_t<1, true> { using type = int8_t; };
template <> struct int_by_bytes_t<2, true> { using type = int16_t; };
template <> struct int_by_bytes_t<4, true> { using type = int32_t; };
template <> struct int_by_bytes_t<8, true> { using type = int64_t; };
template <> struct int_by_bytes_t<16, true> { using type = int128_t; };
// clang-format on

} // namespace detail::integer

/// defines an integer with given size in bytes and signedness
template <int byte_count, bool is_signed>
    requires(byte_count > 0)
using int_by_bytes_t = detail::integer::int_by_bytes_t<byte_count, is_signed>::type;

/// defines an integer with given size in bits and signedness
template <int bit_count, bool is_signed>
    requires(bit_count > 0)
using int_by_bits_t
    = int_by_bytes_t<std::bit_ceil(static_cast<uint_t>(bit_count + CHAR_BIT - 1) / CHAR_BIT), is_signed>;

// --------------------------------------------------------------------------------------------------------------------
// promotions
// --------------------------------------------------------------------------------------------------------------------

/// true when a type can be widened; true up to native word size
template <integral value_t> inline constexpr auto can_widen = sizeof(value_t) <= sizeof(int_t);

/// doubles width of value_t, retaining signedness
template <integral value_t> using widened_t = int_by_bytes_t<sizeof(value_t) * 2, is_signed_v<value_t>>;

/// returns value in widened container
template <integral value_t>
    requires(can_widen<value_t>)
constexpr auto widen(value_t value) noexcept -> widened_t<value_t>
{
    return static_cast<widened_t<value_t>>(value);
}

/// true when a type can be narrowed; true down to 1 byte
template <integral value_t> inline constexpr auto can_narrow = sizeof(value_t) > 1;

/// halves width of value_t, retaining signedness
template <integral value_t> using narrowed_t = int_by_bytes_t<sizeof(value_t) / 2, is_signed_v<value_t>>;

/// returns value in narrowed container, asserting range
template <integral value_t>
    requires(can_narrow<value_t>)
constexpr auto narrow(value_t value) noexcept -> narrowed_t<value_t>
{
    return int_cast<narrowed_t<value_t>>(value);
}

} // namespace crv
