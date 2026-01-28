// SPDX-License-Identifier: MIT
/*!
  \file
  \brief fundamental integer type traits

  \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <crv/math/int_traits.hpp>
#include <cassert>
#include <type_traits>
#include <utility>

namespace crv {

// --------------------------------------------------------------------------------------------------------------------
// int_cast
// --------------------------------------------------------------------------------------------------------------------

//! asserts that from is in the representable range of to_t
template <integral to_t, integral from_t> constexpr auto int_cast(from_t from) noexcept -> to_t
{
    assert(std::in_range<to_t>(from) && "int_cast: input out of range");
    return static_cast<to_t>(from);
}

// --------------------------------------------------------------------------------------------------------------------
// to_unsigned_abs
// --------------------------------------------------------------------------------------------------------------------

//! converts to unsigned type, applying abs when negative
template <signed_integral signed_t>
constexpr auto to_unsigned_abs(signed_t src) noexcept -> std::make_unsigned_t<signed_t>
{
    using unsigned_t = std::make_unsigned_t<signed_t>;
    return src < 0 ? -static_cast<unsigned_t>(src) : static_cast<unsigned_t>(src);
}

} // namespace crv
