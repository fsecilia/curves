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
namespace primitives {

/**
    applies nearest rounding to a division followed by a right shift, avoiding double rounding

    This primitive delegates based on whether there is a shift or remainder:

        !shift:              pure division, no shift; delegates to div
        shift && !remainder: shift is exact after division, pure shift; delegates to shr
        shift && remainder:  remainder breaks any exact mid-point ties

    Because |remainder| < divisor, its fractional contribution is strictly less than one ULP of the shift's fractional
    bits. It can never flip the rounding direction unless the shift's fractional bits land exactly on a midpoint, a tie.

    When a tie occurs, the remainder decides the tie-break direction. Since c++ division truncates toward zero, the
    remainder always shares the sign of the quotient, assuming a positive divisor:
        - A positive quotient has a positive remainder, pushing the value slightly above the midpoint (+carry).
        - A negative quotient has a negative remainder, pushing the value slightly below the midpoint
          (no carry, keeping the floor, which is the more negative value).

    In both cases, a non-zero remainder pushes the magnitude away from zero. This matches shr::nearest_away_t
    exactly for all nearest rounding modes, independently of the parameterized shr and div.
*/
struct div_shr_t
{
    template <typename self_t, integral value_t>
    constexpr auto div_shr(this self_t&& self, value_t shifted_quotient, value_t quotient, value_t divisor,
                           value_t remainder, int_t shift) noexcept -> value_t;
};

} // namespace primitives

/**
    truncates: division truncates toward 0 and shifts floor toward negative infinity

    Standard c++ integer behavior. No correction is applied, making this the fastest mode, but also the most biased.

    This rounding mode is not suitable for signal applications.

    Bias per shift by k:    -(2^k - 1)/2^(k + 1) ulp, approaches -1/2
    Bias per division by d: -(d - 1)/2d ulp, approaches -1/2
    Accumulated error:      linear in N, bias-dominated
*/
struct truncate_t
{
    /// returns shifted unchanged
    template <integral value_t> constexpr auto shr(value_t shifted, value_t, int_t) const noexcept -> value_t
    {
        return shifted;
    }

    /// returns quotient unchanged
    template <integral value_t> constexpr auto div(value_t quotient, value_t, value_t) const noexcept -> value_t
    {
        return quotient;
    }

    // returns shifted_quotient unchanged
    template <integral value_t>
    constexpr auto div_shr(value_t shifted_quotient, value_t, value_t, value_t, int_t) const noexcept -> value_t
    {
        return shifted_quotient;
    }
};
inline constexpr auto truncate = truncate_t{};

/**
    rounds to nearest, breaking ties by rounding toward positive infinity

    This is faster than symmetric and rne, but introduces positive DC bias in signed signals. Appropriate for unsigned
    or positive-only data.

    Bias per shift by k:    +1/2^(k + 1) ulp
    Bias per division by d: +1/2d ulp for even d, 0 for odd d
    Accumulated error:      sqrt(N) at practical sample sizes, linear dominates after N > 2^(2k)/3
*/
struct nearest_up_t : primitives::div_shr_t
{
    /// carry = (unshifted >> (shift - 1)) & 1
    template <integral value_t>
    constexpr auto shr(value_t shifted, value_t unshifted, int_t shift) const noexcept -> value_t
    {
        assert(shift > 0 && "nearest_up_t: shift must be positive");

        auto const carry = static_cast<value_t>((unshifted >> (shift - 1)) & 1);

        return shifted + carry;
    }

    /// carry = (|rem| + positive) > (divisor - |rem|)
    template <signed_integral value_t>
    constexpr auto div(value_t quotient, value_t divisor, value_t remainder) const noexcept -> value_t
    {
        assert(divisor > 0 && "nearest_up_t: divisors must be positive");

        auto const abs_rem  = remainder < 0 ? -remainder : remainder;
        auto const positive = remainder >= 0;
        auto const round    = (abs_rem + positive) > (divisor - abs_rem);
        auto const carry    = round ? (positive ? 1 : -1) : 0;

        return quotient + carry;
    }

    /// unsigned simplifies to carry = (remainder + 1) > (divisor - remainder)
    template <unsigned_integral value_t>
    constexpr auto div(value_t quotient, value_t divisor, value_t remainder) const noexcept -> value_t
    {
        assert(divisor > 0 && "nearest_up_t: divisors must be positive");

        auto const carry = (remainder + 1) > (divisor - remainder);

        return quotient + carry;
    }
};
inline constexpr auto nearest_up = nearest_up_t{};

/**
    rounds to nearest, breaking ties by rounding away from zero

    Slower than nearest_up, but faster than rne. Inflates variance, pushing everything outward from 0. Means of
    perfectly symmetrical data will stay close to 0, but sums of absolute values will drift. Suitable for signed
    signals.

    Bias per shift by k:    +/-1/2^(k + 1) ulp, sign matches input
    Bias per division by d: +/-1/2d ulp for even d, 0 for odd d
    Accumulated error:      sqrt(N) for sign-balanced data, biases cancel; same as nearest_up for same-sign data
*/
struct nearest_away_t : nearest_up_t
{
    using nearest_up_t::div;
    using nearest_up_t::shr;

    /// carry = frac >= (half + negative)
    template <signed_integral value_t>
    constexpr auto shr(value_t shifted, value_t unshifted, int_t shift) const noexcept -> value_t
    {
        assert(shift > 0 && "nearest_away_t: shift must be positive");

        using uval_t       = make_unsigned_t<value_t>;
        constexpr auto one = uval_t{1};

        auto const half     = one << (shift - 1);
        auto const mask     = (one << shift) - 1;
        auto const frac     = static_cast<uval_t>(unshifted) & mask;
        auto const negative = static_cast<uval_t>(unshifted < 0);
        auto const carry    = static_cast<value_t>(frac >= (half + negative));

        return shifted + carry;
    }

    /// carry = |rem| >= (divisor - |rem|)
    template <signed_integral value_t>
    constexpr auto div(value_t quotient, value_t divisor, value_t remainder) const noexcept -> value_t
    {
        assert(divisor > 0 && "nearest_away_t: divisors must be positive");

        auto const abs_rem  = remainder < 0 ? -remainder : remainder;
        auto const positive = remainder >= 0;
        auto const round    = abs_rem >= (divisor - abs_rem);
        auto const carry    = round ? (positive ? 1 : -1) : 0;

        return quotient + carry;
    }
};
inline constexpr auto nearest_away = nearest_away_t{};

/**
    rounds to nearest, breaking ties by rounding to nearest even

    This is rne, also called banker's rounding. Slowest, but unbiased. This is the only mode where expected error is
    exactly zero regardless of data distribution or divisor parity.

    Suitable for recursive filters, feedback loops, long accumulations, or anywhere DC offset matters.

    Bias per operation: 0
    Accumulated error:  sqrt(N)
*/
struct nearest_even_t : primitives::div_shr_t
{
    /// carry = (frac + (half - 1) + (shifted & 1)) >> shift
    template <integral value_t>
    constexpr auto shr(value_t shifted, value_t unshifted, int_t shift) const noexcept -> value_t
    {
        assert(shift > 0 && "nearest_even_t: shift must be positive");

        using uval_t       = make_unsigned_t<value_t>;
        constexpr auto one = uval_t{1};

        auto const half = one << (shift - 1);
        auto const mask = (one << shift) - 1;
        auto const frac = static_cast<uval_t>(unshifted) & mask;

        auto const tiebreaker = static_cast<uval_t>(shifted) & 1;
        auto const bias       = (half - 1) + tiebreaker;
        auto const carry      = static_cast<value_t>((frac + bias) >> shift);

        return shifted + carry;
    }

    /// carry = (|rem| + is_odd) > (divisor - |rem|)
    template <signed_integral value_t>
    constexpr auto div(value_t quotient, value_t divisor, value_t remainder) const noexcept -> value_t
    {
        assert(divisor > 0 && "nearest_even_t: divisors must be positive");

        auto const is_odd   = quotient & 1;
        auto const abs_rem  = remainder < 0 ? -remainder : remainder;
        auto const positive = remainder >= 0;
        auto const round    = (abs_rem + is_odd) > (divisor - abs_rem);
        auto const carry    = round ? (positive ? 1 : -1) : 0;

        return quotient + carry;
    }

    /// unsigned simplifies to carry = (remainder + is_odd) > (divisor - remainder)
    template <unsigned_integral value_t>
    constexpr auto div(value_t quotient, value_t divisor, value_t remainder) const noexcept -> value_t
    {
        assert(divisor > 0 && "nearest_even_t: divisors must be positive");

        auto const is_odd = quotient & 1;
        auto const carry  = (remainder + is_odd) > (divisor - remainder);

        return quotient + carry;
    }
};
inline constexpr auto nearest_even = nearest_even_t{};

namespace primitives {

// --------------------------------------------------------------------------------------------------------------------
// div_shr_t
// --------------------------------------------------------------------------------------------------------------------

template <typename self_t, integral value_t>
constexpr auto div_shr_t::div_shr(this self_t&& self, value_t shifted_quotient, value_t quotient, value_t divisor,
                                  value_t remainder, int_t shift) noexcept -> value_t
{
    if (!shift) return self.div(shifted_quotient, divisor, remainder);
    if (!remainder) return self.shr(shifted_quotient, quotient, shift);
    return nearest_away.shr(shifted_quotient, quotient, shift);
}

} // namespace primitives
} // namespace crv::rounding_modes
