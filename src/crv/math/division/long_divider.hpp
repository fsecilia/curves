// SPDX-License-Identifier: MIT

/// \file
/// \brief unsigned long division
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/division/concepts.hpp>
#include <crv/math/division/result.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <climits>

namespace crv::division {

/// performs unsigned long division, splitting dividend into high and low halves
template <unsigned_integral dividend_t, unsigned_integral divisor_t,
          is_divider<dividend_t, divisor_t> hardware_divider_t>
struct long_divider_t;

/// specializes explicitly on wide dividend, narrow divisor
template <unsigned_integral narrow_t, is_wide_divider<narrow_t> hardware_divider_t>
struct long_divider_t<wider_t<narrow_t>, narrow_t, hardware_divider_t>
{
    using wide_t   = wider_t<narrow_t>;
    using result_t = result_t<wide_t, narrow_t>;

    [[no_unique_address]] hardware_divider_t hardware_divider;

    /// invokes hardware divider with high and low halves of dividend
    ///
    /// This function takes an arbitrary dividend and divisor, splits the dividend into high and low halves, then
    /// performs long division, invoking the hardware divider to divide each half, strictly satisfying the hardware
    /// divider's precondition that the upper half of the passed dividend must be strictly less than the passed divisor.
    constexpr auto operator()(wide_t dividend, narrow_t divisor) const noexcept -> result_t
    {
        // [x] := floor(x)
        // x = [x/y]y + x%y ; division identity
        //
        // a = high
        // b = low
        // c = 1 << shift
        // d = divisor
        // dividend = ac + b
        //     = ([a/d]d + a%d)c + b                               ; apply division identity to a
        //     = [a/d]cd + (a%d)c + b                              ; distribute c
        //     = [a/d]cd + [((a%d)c + b)/d]d + ((a%d)c + b)%d      ; apply division identity to (a%d)c + b
        //     = ([a/d]c + [((a%d)c + b)/d])d + ((a%d)c + b)%d     ; factor out d
        //     = ([a/d]c + [((a%d)c | b)/d])d + ((a%d)c | b)%d     ; b < c, so replace + with |
        //
        // q = ([a/d]c + [((a%d)c | b)/d])
        // r = ((a%d)c | b)%d
        // ac + b = qd + r, r < d
        //
        // For both divisions, upper half of passed dividend is strictly less than passed divisor:
        //     a/d -> (0c | a) >> shift < d
        //     ((a%d)c | b)/d -> ((a%d)c | b) >> shift < d

        constexpr auto const width    = sizeof(dividend) * CHAR_BIT;
        constexpr auto const shift    = width / 2;
        constexpr auto const low_mask = (wide_t{1} << shift) - 1;

        auto const high = dividend >> shift;
        auto const low  = dividend & low_mask;

        // upper half is 0, by definition
        auto const high_result = hardware_divider(high, divisor);

        // upper half is remainder of dividing by divisor, so it is strictly less than divisor
        auto const remaining_result
            = hardware_divider((int_cast<wide_t>(high_result.remainder) << shift) | low, divisor);

        return result_t{.quotient  = (int_cast<wide_t>(high_result.quotient) << shift) + remaining_result.quotient,
                        .remainder = remaining_result.remainder};
    }
};

} // namespace crv::division
