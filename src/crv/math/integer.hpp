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

namespace crv {

// ====================================================================================================================
// Math
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// log2
// --------------------------------------------------------------------------------------------------------------------

template <integral value_t> constexpr auto log2(value_t value) noexcept -> value_t
{
    assert(value > 0 && "log2: domain error");
    return static_cast<value_t>(std::bit_width(value) - 1);
}

// ====================================================================================================================
// Conversions
// ====================================================================================================================

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

} // namespace crv
