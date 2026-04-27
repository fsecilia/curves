// SPDX-License-Identifier: MIT

/// \file
/// \brief std::abs constexpr polyfill
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <concepts>
#include <cstdlib>
#include <type_traits>

namespace crv {

/// constexpr-compatible absolute value fallback
///
/// c++23 added support for constexpr abs(), but compilers have varying levels of support. This implementation uses a
/// ternary for the constexpr path, but falls back to actual std::abs during runtime because it tends to map to a
/// single hardware instruction.
template <typename value_t> constexpr auto abs(value_t value) noexcept -> value_t
{
    if constexpr (std::same_as<value_t, int128_t>)
    {
        // polyfill: when abs(int128_t) lands in the standard, delete this branch

        // std::abs doesn't yet support int128_t, so there's no fallback. The else of `if consteval` can't see int128_t
        // or it will be ambiguous, so handle it directly here.
        return value < 0 ? -value : value;
    }
    else
    {
        if consteval
        {
            // polyfill: when constexpr abs lands in the standard, delete this branch

            if constexpr (std::is_integral_v<value_t>)
            {
                // negating unsigneds causes compiler warnings; std::abs doesn't support unsigned types anyway
                static_assert(std::is_signed_v<value_t>);

                // this will stop compilation if value is min<value_t>() because negating that is unrepresentable
                //
                // This behavior is desirable. If you are here after a compilation error, your min value is oor.
                return value < 0 ? -value : value;
            }
            else
            {
                // if value is NaN, both conditions below are false, so it naturally returns NaN

                // intercept both +0.0 and -0.0, returning a strict +0.0
                if (value == value_t{0}) return value_t{0};

                // floating-point types handle negation symmetrically
                return value < value_t{0} ? -value : value;
            }
        }
        else
        {
            using std::abs;
            return abs(value);
        }
    }
}

} // namespace crv
