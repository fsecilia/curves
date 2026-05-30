// SPDX-License-Identifier: MIT

/// \file
/// \brief bitwise operations
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/int_traits.hpp>
#include <bit>
#include <concepts>

namespace crv {

/// signed bit_width
template <std::signed_integral value_t> constexpr auto bit_width(value_t value) noexcept -> int
{
    using unsigned_t = make_unsigned_t<value_t>;
    auto const abs_value = value < static_cast<value_t>(0) ? static_cast<unsigned_t>(0) - static_cast<unsigned_t>(value)
                                                           : static_cast<unsigned_t>(value);
    return std::bit_width(static_cast<unsigned_t>(abs_value));
}

using std::bit_width;

} // namespace crv
