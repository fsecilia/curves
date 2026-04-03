// SPDX-License-Identifier: MIT

/// \file
/// \brief std::abs constexpr polyfill
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <cstdlib>

namespace crv {

/// \brief constexpr-compatible absolute value fallback
///
/// c++23 added support for constexpr abs(), but compilers have varying levels of support. This implementation uses a
/// ternary for the constexpr path, but falls back to actual std::abs during runtime because it tends to map to a
/// hardware instruction.
template <typename value_t> constexpr auto abs(value_t value) noexcept -> value_t
{
    if consteval
    {
        // if value is NaN, both conditions below are false, so it naturally returns NaN

        // intercept both +0.0 and -0.0, returning a strict +0.0
        if (value == value_t{0}) return value_t{0};

        // handle positive/negative normally.
        return value < value_t{0} ? -value : value;
    }
    else
    {
        using std::abs;
        return abs(value);
    }
}

} // namespace crv
