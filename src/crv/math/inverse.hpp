// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <cassert>
#include <concepts>
#include <numeric>

namespace crv {

/// floating-point lower-bound bisection for monotonic increasing functions
struct bisect_lower_bound_t
{
    /// \returns the leftmost x in [low, high] where f(x) >= target
    /// \pre monotone_t is monotonic increasing
    /// \pre low <= high
    template <std::floating_point real_t, typename monotone_t>
    [[nodiscard]] constexpr auto operator()(real_t low, real_t high, real_t target, monotone_t const& f) const noexcept
        -> real_t
    {
        assert(low <= high);

        // handle low oor
        //
        // Without this check, low oor returns the other side of the interval.
        if (target <= f(low)) return low;

        while (true)
        {
            auto const mid = std::midpoint(low, high);
            if (mid == low || mid == high) break;

            if (f(mid) < target) low = mid;
            else high = mid;
        }

        return high;
    }
};

} // namespace crv
