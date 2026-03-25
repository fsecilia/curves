// SPDX-License-Identifier: MIT

/// \file
/// \brief unsigned long division
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/division/concepts.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/rounding_mode.hpp>
#include <climits>

namespace crv::division {

/// performs unsigned long division, splitting dividend into high and low halves
template <unsigned_integral dividend_t, unsigned_integral divisor_t, typename hardware_divider_t> struct long_divider_t;

/// divides a wide dividend by a narrow divisor, then rounds using a rounding mode; returns wide quotient
template <unsigned_integral narrow_t, is_hardware_divider<narrow_t> hardware_divider_t> struct wide_divider_t
{
    using wide_t = wider_t<narrow_t>;

    static constexpr auto const narrow_width = static_cast<int_t>(sizeof(narrow_t) * CHAR_BIT);

    [[no_unique_address]] hardware_divider_t hardware_divider;

    /// brackets division with rounding mode
    ///
    /// This method applies the rounding mode's bias before division and carry after. It dispatches directly to hardware
    /// division when the quotient fits in a narrow register and falls back to long division otherwise. The carry is
    /// applied inside each branch to preserve the narrow-range information for the optimizer.
    template <is_div_rounding_mode<wide_t, narrow_t> rounding_mode_t>
    constexpr auto operator()(wide_t dividend, narrow_t divisor, rounding_mode_t rounding_mode) const noexcept -> wide_t
    {
        auto const biased        = rounding_mode.bias(dividend, divisor);
        auto const high_word     = static_cast<narrow_t>(biased >> narrow_width);
        auto const quotient_fits = high_word < divisor;
        if (quotient_fits) return apply_hardware_division(biased, divisor, rounding_mode);
        else return apply_long_division(biased, divisor, rounding_mode);
    }

private:
    // invokes hardware divider directly
    template <is_div_rounding_mode<wide_t, narrow_t> rounding_mode_t>
    constexpr auto apply_hardware_division(wide_t dividend, narrow_t divisor,
                                           rounding_mode_t rounding_mode) const noexcept -> wide_t
    {
        auto const result = hardware_divider(dividend, divisor);
        return rounding_mode.carry(int_cast<wide_t>(result.quotient), divisor, result.remainder);
    }

    // invokes hardware divider with high and low halves of dividend
    //
    // This method takes an arbitrary dividend and divisor, splits the dividend into high and low halves, then performs
    // long division, invoking the hardware divider to divide each half, strictly satisfying the hardware divider's
    // precondition that the upper half of the passed dividend must be strictly less than the passed divisor.
    template <is_div_rounding_mode<wide_t, narrow_t> rounding_mode_t>
    constexpr auto apply_long_division(wide_t dividend, narrow_t divisor, rounding_mode_t rounding_mode) const noexcept
        -> wide_t
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

        constexpr auto const low_mask = int_cast<wide_t>((wide_t{1} << narrow_width) - 1);

        auto const high_dividend_high = int_cast<narrow_t>(dividend >> narrow_width);
        auto const high_dividend_low  = int_cast<narrow_t>(dividend & low_mask);

        // upper half is 0, by definition
        auto const high_result = hardware_divider(high_dividend_high, divisor);

        // upper half is remainder of dividing by divisor, so it is strictly less than divisor
        auto const low_dividend = (int_cast<wide_t>(high_result.remainder) << narrow_width) | high_dividend_low;
        auto const low_result   = hardware_divider(low_dividend, divisor);

        auto const quotient
            = int_cast<wide_t>((int_cast<wide_t>(high_result.quotient) << narrow_width) + low_result.quotient);
        return rounding_mode.carry(quotient, divisor, low_result.remainder);
    }
};

} // namespace crv::division
