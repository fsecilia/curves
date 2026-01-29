// SPDX-License-Identifier: MIT
/*!
  \file
  \brief integer fundamentals

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/limits.hpp>
#include <bit>
#include <cassert>
#include <type_traits>
#include <utility>

namespace crv {

// ====================================================================================================================
// Math
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// log2
// --------------------------------------------------------------------------------------------------------------------

template <unsigned_integral value_t> constexpr auto log2(value_t value) noexcept -> value_t
{
    assert(value != 0 && "log2: domain error");
    return std::bit_width(value) - 1;
}

// ====================================================================================================================
// Conversions
// ====================================================================================================================

//! asserts that from is in the representable range of to_t
template <integral to_t, integral from_t> constexpr auto int_cast(from_t from) noexcept -> to_t
{
    assert(std::in_range<to_t>(from) && "int_cast: input out of range");
    return static_cast<to_t>(from);
}

//! converts to unsigned type, applying abs when negative
template <signed_integral signed_t>
constexpr auto to_unsigned_abs(signed_t src) noexcept -> std::make_unsigned_t<signed_t>
{
    using unsigned_t = std::make_unsigned_t<signed_t>;
    return src < 0 ? -static_cast<unsigned_t>(src) : static_cast<unsigned_t>(src);
}

//! converts to signed type, applying sign of sign to result
template <unsigned_integral unsigned_t>
constexpr auto to_signed_copysign(unsigned_t src, signed_integral auto sign) noexcept -> std::make_signed_t<unsigned_t>
{
    using dst_t = std::make_signed_t<unsigned_t>;

    // result must be within [min(), max()] for signed range
    assert(src <= (sign < 0 ? static_cast<unsigned_t>(min<dst_t>()) : static_cast<unsigned_t>(max<dst_t>()))
           && "to_signed_copysign: input out of range");

    return sign < 0 ? static_cast<dst_t>(-src) : static_cast<dst_t>(src);
}

} // namespace crv
