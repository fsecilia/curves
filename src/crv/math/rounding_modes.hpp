// SPDX-License-Identifier: MIT
/**
    \file
    \brief integer rounding modes after right shift and division

    These rounding modes take an integer result of right shift, division, or a division followed by a right shift, and
    conditionally correct them without double rounding by adding +1, -1, or 0.

    Most of these use a trick to check for more than half, avoiding overflow:

        rem > div/2 <=> 2*rem > div <=> rem > (div - rem)

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <crv/math/int_traits.hpp>
#include <cassert>

namespace crv::rounding_modes {

/**
    truncates: division truncates toward 0 and shifts floor toward negative infinity

    Standard c++ integer behavior. No correction is applied, making this the fastest mode, but also the most biased.

    This rounding mode is not suitable for signal applications.

    Bias per shift by k:    -(2^k - 1)/2^(k + 1) ulp, approaches -1/2
    Bias per division by d: -(d - 1)/2d ulp, approaches -1/2
    Accumulated error:      linear in N, bias-dominated
*/
struct truncate
{
    template <integral value_t> constexpr auto shr(value_t shifted, value_t, int_t) const noexcept -> value_t
    {
        return shifted;
    }

    template <integral value_t> constexpr auto div(value_t quotient, value_t, value_t) const noexcept -> value_t
    {
        return quotient;
    }

    template <integral value_t>
    constexpr auto div_shr(value_t shifted_quotient, value_t, value_t, value_t, int_t) const noexcept -> value_t
    {
        return shifted_quotient;
    }
};

/**
    rounds to nearest, breaking ties by rounding toward positive infinity

    This is faster than symmetric and rne, but introduces positive DC bias in signed signals. Appropriate for unsigned
    or positive-only data.

    Bias per shift by k:    +1/2^(k + 1) ulp
    Bias per division by d: +1/2d ulp for even d, 0 for odd d
    Accumulated error:      sqrt(N) at practical sample sizes, linear dominates after N > 2^(2k)/3
*/
struct asymmetric
{
    template <integral value_t>
    constexpr auto shr(value_t shifted, value_t unshifted, int_t shift) const noexcept -> value_t
    {
        assert(shift > 0 && "asymmetric::shr: shift must be positive");

        auto const carry = (unshifted >> (shift - 1)) & 1;

        return shifted + carry;
    }

    template <integral value_t>
    constexpr auto div(value_t quotient, value_t divisor, value_t remainder) const noexcept -> value_t
    {
        assert(divisor > 0 && "asymmetric::div: divisors must be positive");

        if constexpr (is_signed_v<value_t>)
        {
            auto const abs_rem  = remainder < 0 ? -remainder : remainder;
            auto const positive = remainder >= 0;
            auto const round    = (abs_rem + positive) > (divisor - abs_rem);
            auto const carry    = round ? (positive ? 1 : -1) : 0;

            return quotient + carry;
        }
        else
        {
            auto const carry = (remainder + 1) > (divisor - remainder);

            return quotient + carry;
        }
    }

    template <integral value_t>
    constexpr auto div_shr(value_t shifted_quotient, value_t quotient, value_t divisor, value_t remainder,
                           int_t shift) const noexcept -> value_t
    {
        if (!shift) return div(shifted_quotient, divisor, remainder);
        if (!remainder) return shr(shifted_quotient, quotient, shift);

        if constexpr (is_signed_v<value_t>)
        {
            using uval_t = make_unsigned_t<value_t>;

            constexpr auto one = uval_t{1};

            auto const half     = one << (shift - 1);
            auto const mask     = (one << shift) - 1;
            auto const frac     = static_cast<uval_t>(quotient) & mask;
            auto const negative = static_cast<uval_t>(quotient < 0);
            auto const carry    = static_cast<value_t>(frac >= (half + negative));

            return shifted_quotient + carry;
        }
        else
        {
            return shr(shifted_quotient, quotient, shift);
        }
    }
};

/**
    rounds to nearest, breaking ties by rounding away from zero

    Slower than asymmetric, but faster than rne. Inflates variance, pushing everything outward from 0. Means of
    perfectly symmetrical data will stay close to 0, but sums of absolute values will drift. Suitable for signed
    signals.

    Bias per shift by k:    +/-1/2^(k + 1) ulp, sign matches input
    Bias per division by d: +/-1/2d ulp for even d, 0 for odd d
    Accumulated error:      sqrt(N) for sign-balanced data, biases cancel; same as asymmetric for same-sign data
*/
struct symmetric
{
    template <integral value_t>
    constexpr auto shr(value_t shifted, value_t unshifted, int_t shift) const noexcept -> value_t
    {
        assert(shift > 0 && "symmetric::shr: shift must be positive");

        if constexpr (is_signed_v<value_t>)
        {
            using uval_t = make_unsigned_t<value_t>;

            constexpr auto one = uval_t{1};

            auto const half     = one << (shift - 1);
            auto const mask     = (one << shift) - 1;
            auto const frac     = static_cast<uval_t>(unshifted) & mask;
            auto const negative = static_cast<uval_t>(unshifted < 0);
            auto const carry    = static_cast<value_t>(frac >= (half + negative));

            return shifted + carry;
        }
        else
        {
            auto const carry = (unshifted >> (shift - 1)) & 1;

            return shifted + carry;
        }
    }

    template <integral value_t>
    constexpr auto div(value_t quotient, value_t divisor, value_t remainder) const noexcept -> value_t
    {
        assert(divisor > 0 && "symmetric::div: divisors must be positive");

        if constexpr (is_signed_v<value_t>)
        {
            auto const abs_rem  = remainder < 0 ? -remainder : remainder;
            auto const positive = remainder >= 0;
            auto const round    = abs_rem >= (divisor - abs_rem);
            auto const carry    = round ? (positive ? 1 : -1) : 0;

            return quotient + carry;
        }
        else
        {
            auto const carry = (remainder + 1) > (divisor - remainder);

            return quotient + carry;
        }
    }

    template <integral value_t>
    constexpr auto div_shr(value_t shifted_quotient, value_t quotient, value_t divisor, value_t remainder,
                           int_t shift) const noexcept -> value_t
    {
        if (shift) return shr(shifted_quotient, quotient, shift);
        else return div(shifted_quotient, divisor, remainder);
    }
};

} // namespace crv::rounding_modes
