// SPDX-License-Identifier: MIT

/// \file
/// \brief integer rounding modes for right shift and division
///
/// This module contains rounding modes that may increment or decrement the result of a right shift or division.
/// Unqualified modes conditionally apply a carry after the operation. This is always safe to do and never results in an
/// overflow. Modes in namespace fast bias the input value before the operation to achieve the same result. This is
/// faster, but requires enough headroom to avoid overflow.
///
/// Each mode exposes a pair of apis per operation, bias and carry. Safe rounding modes implement carry and their bias
/// is a passthrough. Fast rounding modes implement bias and their carry is a passthrough. Library implementation sites
/// call both. Users pass rounding mode cpos as black boxes.
///
/// Carries use a trick to check for more than half without overflowing:
///
///     rem > div/2 <=> 2*rem > div <=> rem > (div - rem)
///
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/limits.hpp>
#include <cassert>

namespace crv {

// ====================================================================================================================
// Concepts
// ====================================================================================================================

template <typename rounding_mode_t, typename wide_t>
concept is_shr_rounding_mode = requires(rounding_mode_t const& rounding_mode, wide_t wide, int_t shift) {
    { rounding_mode.bias(wide, shift) } -> std::same_as<wide_t>;
    { rounding_mode.carry(wide, wide, shift) } -> std::same_as<wide_t>;
};

template <typename rounding_mode_t, typename wide_t, typename narrow_t>
concept is_div_rounding_mode = requires(rounding_mode_t const& rounding_mode, wide_t wide, narrow_t narrow) {
    { rounding_mode.bias(wide, narrow) } -> std::same_as<wide_t>;
    { rounding_mode.carry(wide, narrow, narrow) } -> std::same_as<wide_t>;
};

namespace rounding_modes {

// ====================================================================================================================
// Right Shift Rounding Modes
// ====================================================================================================================

namespace shr {

// --------------------------------------------------------------------------------------------------------------------
// Safe Rounding Modes
//
// These round via carry, which is always safe and cannot overflow the given value type.
// --------------------------------------------------------------------------------------------------------------------

/// truncates: floors toward negative infinity
///
/// Standard c++ integer behavior. No correction is applied, making this the fastest mode, but also the most biased.
///
/// This rounding mode is not suitable for signal applications.
///
/// Bias per shift by k: -(2^k - 1)/2^(k + 1) ulp, approaches -1/2
/// Accumulated error:   linear in N, bias-dominated
struct truncate_t
{
    /// returns unshifted unchanged
    template <integral value_t> constexpr auto bias(value_t unshifted, int_t) const noexcept -> value_t
    {
        return unshifted;
    }

    /// returns shifted unchanged
    template <integral value_t> constexpr auto carry(value_t shifted, value_t, int_t) const noexcept -> value_t
    {
        return shifted;
    }
};
inline constexpr auto truncate = truncate_t{};

/// rounds to nearest, breaking ties by rounding toward positive infinity
///
/// This is faster than symmetric and rne, but introduces positive DC bias in signed signals. Appropriate for unsigned
/// or positive-only data.
///
/// Bias per shift by k: +1/2^(k + 1) ulp
/// Accumulated error:   sqrt(N) at practical sample sizes, linear dominates after N > 2^(2k)/3
struct nearest_up_t
{
    /// returns unshifted unchanged
    template <integral value_t> constexpr auto bias(value_t unshifted, int_t) const noexcept -> value_t
    {
        return unshifted;
    }

    /// carry = (unshifted >> (shift - 1)) & 1
    template <integral value_t>
    constexpr auto carry(value_t shifted, value_t unshifted, int_t shift) const noexcept -> value_t
    {
        assert(shift > 0 && "nearest_up_t: shift must be positive");

        auto const carry = static_cast<value_t>((unshifted >> (shift - 1)) & 1);

        return int_cast<value_t>(shifted + carry);
    }
};
inline constexpr auto nearest_up = nearest_up_t{};

/// rounds to nearest, breaking ties by rounding away from zero
///
/// Slower than nearest_up, but faster than rne. Inflates variance, pushing everything outward from 0. Perfectly
/// symmetrical data will stay close to 0, but sums of absolute values will drift. Suitable for signed signals.
///
/// Bias per shift by k: +/-1/2^(k + 1) ulp, sign matches input
/// Accumulated error:   sqrt(N) for sign-balanced data, biases cancel; same as nearest_up for same-sign data
struct nearest_away_t : private nearest_up_t
{
    // biases are the same passthrough
    using nearest_up_t::bias;

    // unsigned carries are the same
    using nearest_up_t::carry;

    /// carry = frac >= (half + negative)
    template <signed_integral value_t>
    constexpr auto carry(value_t shifted, value_t unshifted, int_t shift) const noexcept -> value_t
    {
        assert(shift > 0 && "nearest_away_t: shift must be positive");

        using uval_t       = make_unsigned_t<value_t>;
        constexpr auto one = uval_t{1};

        auto const half = one << (shift - 1);
        auto const mask = (one << shift) - 1;

        auto const frac     = static_cast<uval_t>(unshifted) & mask;
        auto const negative = static_cast<uval_t>(unshifted < 0);
        auto const carry    = static_cast<value_t>(frac >= (half + negative));

        return int_cast<value_t>(shifted + carry);
    }
};
inline constexpr auto nearest_away = nearest_away_t{};

/// rounds to nearest, breaking ties by rounding to nearest even
///
/// This is rne, also called banker's rounding. Slowest, but unbiased. It is the only mode where expected error is
/// exactly zero regardless of data distribution or divisor parity.
///
/// Suitable for recursive filters, feedback loops, long accumulations, or anywhere DC offset matters.
///
/// Bias per shift by k: 0 ulp
/// Accumulated error:   sqrt(N)
struct nearest_even_t
{
    /// returns unshifted unchanged
    template <integral value_t> constexpr auto bias(value_t unshifted, int_t) const noexcept -> value_t
    {
        return unshifted;
    }

    /// carry = (frac + (half - 1) + (shifted & 1)) >> shift
    template <integral value_t>
    constexpr auto carry(value_t shifted, value_t unshifted, int_t shift) const noexcept -> value_t
    {
        assert(shift > 0 && "nearest_even_t: shift must be positive");

        using uval_t       = make_unsigned_t<value_t>;
        constexpr auto one = uval_t{1};

        auto const half = one << (shift - 1);
        auto const mask = (one << shift) - 1;

        auto const frac   = static_cast<uval_t>(unshifted) & mask;
        auto const is_odd = static_cast<uval_t>(shifted) & 1;
        auto const carry  = static_cast<value_t>((frac + is_odd) > half);

        return int_cast<value_t>(shifted + carry);
    }
};
inline constexpr auto nearest_even = nearest_even_t{};

// --------------------------------------------------------------------------------------------------------------------
// Fast Shr Rounding Modes
//
// These round by biasing the value beforehand, which may overflow if there is not guaranteed headroom available.
// --------------------------------------------------------------------------------------------------------------------

namespace fast {

/// fast, biasing version of nearest up
///
/// The caller must guarantee that biased values do not overflow the type.
struct nearest_up_t
{
    /// adds half before shifting
    ///
    /// \pre unshifted + (1 << (shift - 1)) does not overflow
    template <integral value_t> constexpr auto bias(value_t unshifted, int_t shift) const noexcept -> value_t
    {
        assert(shift > 0 && "shr::fast::nearest_up_t::bias: shift must be positive");

        auto const half = static_cast<value_t>(value_t{1} << (shift - 1));

        assert(max<value_t>() - half >= unshifted && "shr::fast::nearest_up_t::bias: integer overflow");

        return int_cast<value_t>(unshifted + half);
    }

    /// pass-through: bias already applied
    template <integral value_t> constexpr auto carry(value_t shifted, value_t, int_t) const noexcept -> value_t
    {
        return shifted;
    }
};
inline constexpr auto nearest_up = nearest_up_t{};

/// fast, biasing version of nearest away
///
/// The caller must guarantee that biased values do not overflow the type.
struct nearest_away_t
{
    /// unsigned: add half before shifting
    ///
    /// \pre unshifted + (1 << (shift - 1)) does not overflow
    template <unsigned_integral value_t> constexpr auto bias(value_t unshifted, int_t shift) const noexcept -> value_t
    {
        assert(shift > 0 && "fast::nearest_away_t: shift must be positive");

        auto const half = static_cast<value_t>(value_t{1} << (shift - 1));

        assert(max<value_t>() - half >= unshifted && "fast::nearest_away_t::shr_bias: integer overflow");

        return int_cast<value_t>(unshifted + half);
    }

    /// signed: bias is half for positive, half - 1 for negative (ties go away from zero)
    ///
    /// \pre unshifted + (1 << (shift - 1)) does not overflow
    template <signed_integral value_t> constexpr auto bias(value_t unshifted, int_t shift) const noexcept -> value_t
    {
        assert(shift > 0 && "fast::nearest_away_t: shift must be positive");

        using uval_t    = make_unsigned_t<value_t>;
        auto const half = static_cast<uval_t>(uval_t{1} << (shift - 1));
        auto const bias = static_cast<value_t>(half - static_cast<uval_t>(unshifted < 0));

        assert(max<value_t>() - bias >= unshifted && "fast::nearest_away_t::shr_bias: integer overflow");

        return int_cast<value_t>(unshifted + bias);
    }

    /// pass-through: bias already applied
    template <integral value_t> constexpr auto carry(value_t shifted, value_t, int_t) const noexcept -> value_t
    {
        return shifted;
    }
};
inline constexpr auto nearest_away = nearest_away_t{};

} // namespace fast
} // namespace shr

// ====================================================================================================================
// Division Rounding Modes
//
// Division operates on unsigned values after stripping sign. The divider restores sign to the quotient and remainder
// after rounding. Because rounding is applied to unsigned intermediates, there is no nearest_up.
// ====================================================================================================================

namespace div {

// --------------------------------------------------------------------------------------------------------------------
// Safe Rounding Modes
//
// These round via carry, which is always safe and cannot overflow the given value type.
// --------------------------------------------------------------------------------------------------------------------

/// truncates: truncates toward 0
///
/// Standard c++ integer behavior. No correction is applied, making this the fastest mode, but also the most biased.
///
/// This rounding mode is not suitable for signal applications.
///
/// Bias per division by d: -(d - 1)/2d ulp, approaches -1/2
/// Accumulated error:      linear in N, bias-dominated
struct truncate_t
{
    /// returns dividend unchanged
    template <unsigned_integral wide_t, unsigned_integral narrow_t>
        requires(std::is_signed_v<wide_t> == std::is_signed_v<narrow_t>)
    constexpr auto bias(wide_t dividend, narrow_t) const noexcept -> wide_t
    {
        return dividend;
    }

    /// returns quotient unchanged
    template <unsigned_integral wide_t, unsigned_integral narrow_t>
        requires(std::is_signed_v<wide_t> == std::is_signed_v<narrow_t>)
    constexpr auto carry(wide_t quotient, narrow_t, narrow_t) const noexcept -> wide_t
    {
        return quotient;
    }
};
inline constexpr auto truncate = truncate_t{};

/// rounds to nearest, breaking ties by rounding away from zero
///
/// Slower than truncate, but faster than rne. Inflates variance, pushing everything outward from 0. Means of perfectly
/// symmetrical data will stay close to 0, but sums of absolute values will drift. Suitable for signed signals.
///
/// Bias per division by d: +/-1/2d ulp for even d, 0 for odd d
/// Accumulated error:      sqrt(N) for practical sample sizes; linear dominates same-sign data after N > 2^(2k)/3
///                         sign-balanced data cancels bias
struct nearest_away_t
{
    /// returns dividend unchanged
    template <unsigned_integral wide_t, unsigned_integral narrow_t>
        requires(std::is_signed_v<wide_t> == std::is_signed_v<narrow_t>)
    constexpr auto bias(wide_t dividend, narrow_t) const noexcept -> wide_t
    {
        return dividend;
    }

    /// unsigned simplifies to carry = (remainder + 1) > (divisor - remainder)
    template <unsigned_integral wide_t, unsigned_integral narrow_t>
    constexpr auto carry(wide_t quotient, narrow_t divisor, narrow_t remainder) const noexcept -> wide_t
    {
        assert(divisor > 0 && "nearest_away_t: divisors must be positive");

        auto const carry = (remainder + 1) > (divisor - remainder);

        return int_cast<wide_t>(quotient + carry);
    }
};
inline constexpr auto nearest_away = nearest_away_t{};

/// rounds to nearest, breaking ties by rounding to nearest even
///
/// This is rne, also called banker's rounding. Slowest, but unbiased. It is the only mode where expected error is
/// exactly zero regardless of data distribution or divisor parity.
///
/// Suitable for recursive filters, feedback loops, long accumulations, or anywhere DC offset matters.
///
/// Bias per division by d: 0 ulp
/// Accumulated error:      sqrt(N)
struct nearest_even_t
{
    /// returns dividend unchanged
    template <unsigned_integral wide_t, unsigned_integral narrow_t>
        requires(std::is_signed_v<wide_t> == std::is_signed_v<narrow_t>)
    constexpr auto bias(wide_t dividend, narrow_t) const noexcept -> wide_t
    {
        return dividend;
    }

    /// unsigned simplifies to carry = (remainder + is_odd) > (divisor - remainder)
    template <unsigned_integral wide_t, unsigned_integral narrow_t>
    constexpr auto carry(wide_t quotient, narrow_t divisor, narrow_t remainder) const noexcept -> wide_t
    {
        assert(divisor > 0 && "nearest_even_t: divisors must be positive");

        auto const is_odd = quotient & 1;
        auto const carry  = (remainder + is_odd) > (divisor - remainder);

        return int_cast<wide_t>(quotient + carry);
    }
};
inline constexpr auto nearest_even = nearest_even_t{};

// --------------------------------------------------------------------------------------------------------------------
// Fast Rounding Modes
//
// These round by biasing the value beforehand, which may overflow if there is not guaranteed headroom available.
// --------------------------------------------------------------------------------------------------------------------

namespace fast {

/// fast version of nearest away
///
/// Implements biasing version of shr and div. Delegates div_shr because there is no fast version of div_shr.
///
/// The caller must guarantee that biased values do not overflow the type.
struct nearest_away_t
{
    /// add floor(divisor/2) before dividing
    ///
    /// \pre dividend + (divisor >> 1) does not overflow
    template <unsigned_integral wide_t, unsigned_integral narrow_t>
    constexpr auto bias(wide_t dividend, narrow_t divisor) const noexcept -> wide_t
    {
        assert(divisor > 0 && "fast::nearest_away_t: divisor must be positive");

        auto const half = divisor >> 1;

        assert(max<wide_t>() - half >= dividend && "fast::nearest_away_t::div_bias: integer overflow");

        return int_cast<wide_t>(dividend + half);
    }

    /// pass-through: bias already applied
    template <unsigned_integral wide_t, unsigned_integral narrow_t>
    constexpr auto carry(wide_t quotient, narrow_t, narrow_t) const noexcept -> wide_t
    {
        return quotient;
    }
};
inline constexpr auto nearest_away = nearest_away_t{};

} // namespace fast
} // namespace div
} // namespace rounding_modes
} // namespace crv
