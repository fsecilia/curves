// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/integer.hpp>
#include <bit>
#include <cassert>
#include <concepts>
#include <optional>

namespace crv {

/// floating-point lower-bound bisection for monotonic increasing functions
struct bisect_lower_bound_t
{
    /// applies lower-bound bisection to find target in [low, high]
    ///
    /// \returns the leftmost x in [low, high] where f(x) >= target, if any; nullopt otherwise
    /// \pre monotone_t is monotonic increasing
    /// \pre low <= high
    /// \pre low and high must be positve, 0.0, or -0.0
    template <std::floating_point real_t, typename monotone_t>
    [[nodiscard]] constexpr auto operator()(real_t low, real_t high, real_t target, monotone_t const& f) const noexcept
        -> std::optional<real_t>
    {
        assert(low >= real_t{0.0} && "domain must be strictly positive");
        assert(low <= high && "invalid search range");

        // This implementation relies on the fact that positive negative numbers are ordered the same their unsigned
        // integer representation, even though the interpretations are not 1:1. By converting to integer and conducting
        // the search there, converting back to form the function parameter for comparison, then again when the results
        // match identically, the search terminates in less than O(w), where w is the bit width of the type, rather than
        // the thousands of iterations it can take midpointing in float to fp collapse. It also sidesteps the case where
        // two values differ by 1 float ulp and cannot be resolved.

        // handle oor
        if (f(high) < target) return std::nullopt;
        if (target <= f(low)) return low;

        // sanitize negative zeros
        //
        // In float, -0.0 == 0.0, but -0.0 has the negative bit set, breaking the integer-based search here. These
        // ternaries convert -0.0 to 0.0.
        low = (low == real_t{0.0}) ? real_t{0.0} : low;
        high = (high == real_t{0.0}) ? real_t{0.0} : high;

        using unsigned_t = int_by_bytes_t<sizeof(real_t), false>;
        auto low_u = std::bit_cast<unsigned_t>(low);
        auto high_u = std::bit_cast<unsigned_t>(high);

        while (low_u < high_u)
        {
            auto const mid_u = low_u + (high_u - low_u) / 2;
            auto const mid = std::bit_cast<real_t>(mid_u);

            if (f(mid) < target) low_u = mid_u + 1;
            else high_u = mid_u;
        }

        return std::bit_cast<real_t>(high_u);
    }
};

} // namespace crv
