// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "rounding_mode.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <concepts>

namespace crv {
namespace rounding_modes {
namespace {

// ====================================================================================================================
// Concepts
// ====================================================================================================================

struct arbitrary_t
{};

// is_shr_rounding_mode
static_assert(is_shr_rounding_mode<nearest_up_t, int_t, int_t>);     // uniform types
static_assert(is_shr_rounding_mode<nearest_up_t, int32_t, int16_t>); // wide, narow
static_assert(is_shr_rounding_mode<nearest_away_t, int_t, int_t>);   // different rounding mode
static_assert(is_shr_rounding_mode<nearest_up_t, int_t, uint_t>);    // sign difference
static_assert(is_shr_rounding_mode<nearest_up_t, uint_t, int_t>);    // sign difference, reversed
static_assert(!is_shr_rounding_mode<arbitrary_t, int_t, int_t>);     // not a rounding mode

// is_div_rounding_mode
static_assert(is_div_rounding_mode<nearest_up_t, int_t, int_t>);     // uniform types
static_assert(is_div_rounding_mode<nearest_up_t, int32_t, int16_t>); // wide, narow
static_assert(is_div_rounding_mode<nearest_away_t, int_t, int_t>);   // different rounding mode
static_assert(!is_div_rounding_mode<nearest_up_t, int_t, uint_t>);   // sign difference
static_assert(!is_div_rounding_mode<nearest_up_t, uint_t, int_t>);   // sign difference, reversed
static_assert(!is_div_rounding_mode<arbitrary_t, int_t, int_t>);     // not a rounding mode

// is_rounding_mode
static_assert(is_rounding_mode<nearest_up_t, int_t, int_t>);     // uniform types
static_assert(is_rounding_mode<nearest_up_t, int32_t, int16_t>); // wide, narow
static_assert(is_rounding_mode<nearest_away_t, int_t, int_t>);   // different rounding mode
static_assert(!is_rounding_mode<nearest_up_t, int_t, uint_t>);   // sign difference
static_assert(!is_rounding_mode<nearest_up_t, uint_t, int_t>);   // sign difference, reversed
static_assert(!is_rounding_mode<arbitrary_t, int_t, int_t>);     // not a rounding mode

// is_uniform_rounding_mode
static_assert(is_uniform_rounding_mode<nearest_up_t, int_t>);   // signed
static_assert(is_uniform_rounding_mode<nearest_up_t, uint_t>);  // unsigned
static_assert(is_uniform_rounding_mode<nearest_away_t, int_t>); // different rounding mode
static_assert(!is_uniform_rounding_mode<arbitrary_t, int_t>);   // not a rounding mode

// ====================================================================================================================
// Rounding Modes Support
// ====================================================================================================================

/// calls shr_carry directly with the arithmetic-shifted value
template <typename value_t, typename sut_t>
constexpr auto shr_carry(value_t unshifted, int_t shift, sut_t sut) -> value_t
{
    return sut.shr_carry(static_cast<value_t>(unshifted >> shift), unshifted, shift);
}

/// composed shr: bias -> shift -> carry
template <typename value_t, typename sut_t> constexpr auto shr(value_t unshifted, int_t shift, sut_t sut) -> value_t
{
    auto const biased  = sut.shr_bias(unshifted, shift);
    auto const shifted = static_cast<value_t>(biased >> shift);
    return sut.shr_carry(shifted, biased, shift);
}

/// calls div_carry directly with the truncated quotient and remainder
template <typename value_t, typename sut_t>
constexpr auto div_carry(value_t dividend, value_t divisor, sut_t sut) -> value_t
{
    return sut.div_carry(static_cast<value_t>(dividend / divisor), divisor, static_cast<value_t>(dividend % divisor));
}

/// composed div: bias -> divide -> carry
template <typename value_t, typename sut_t> constexpr auto div(value_t dividend, value_t divisor, sut_t sut) -> value_t
{
    auto const biased    = sut.div_bias(dividend, divisor);
    auto const quotient  = static_cast<value_t>(biased / divisor);
    auto const remainder = static_cast<value_t>(biased % divisor);
    return sut.div_carry(quotient, divisor, remainder);
}

/// div_shr: truncated division then dispatch
template <typename value_t, typename sut_t>
constexpr auto div_shr(value_t dividend, value_t divisor, int_t shift, sut_t sut) -> value_t
{
    auto const quotient         = static_cast<value_t>(dividend / divisor);
    auto const remainder        = static_cast<value_t>(dividend % divisor);
    auto const shifted_quotient = static_cast<value_t>(quotient >> shift);

    return sut.div_shr(shifted_quotient, quotient, divisor, remainder, shift);
}

/// Tests that apis return the parameterized type, even in the face of small integer promotion rules.
struct preserves_type_t
{
    constexpr auto shr_bias(auto rounding_mode) const noexcept -> bool
    {
        return std::same_as<uint8_t, decltype(rounding_mode.shr_bias(std::declval<uint8_t>(), std::declval<int_t>()))>;
    }

    constexpr auto shr_carry(auto rounding_mode) const noexcept -> bool
    {
        return std::same_as<uint8_t, decltype(rounding_mode.shr_carry(std::declval<uint8_t>(), std::declval<uint8_t>(),
                                                                      std::declval<int_t>()))>;
    }

    constexpr auto div_bias(auto rounding_mode) const noexcept -> bool
    {
        return std::same_as<uint8_t,
                            decltype(rounding_mode.div_bias(std::declval<uint8_t>(), std::declval<uint8_t>()))>;
    }

    constexpr auto div_carry(auto rounding_mode) const noexcept -> bool
    {
        return std::same_as<uint8_t, decltype(rounding_mode.div_bias(std::declval<uint8_t>(), std::declval<uint8_t>(),
                                                                     std::declval<uint8_t>()))>;
    }

    constexpr auto div_shr(auto rounding_mode) const noexcept -> bool
    {
        return std::same_as<uint8_t, decltype(rounding_mode.div_shr(std::declval<uint8_t>(), std::declval<uint8_t>(),
                                                                    std::declval<uint8_t>(), std::declval<uint8_t>(),
                                                                    std::declval<int_t>()))>;
    }

    constexpr auto operator()(auto rounding_mode) const noexcept -> bool
    {
        return shr_bias(rounding_mode) && shr_carry(rounding_mode) && div_bias(rounding_mode) && div_shr(rounding_mode);
    }
};
constexpr auto preserves_type = preserves_type_t{};

// ====================================================================================================================
// truncate_t
// ====================================================================================================================

static_assert(preserves_type(truncate));

// shr_bias: pass-through
static_assert(truncate.shr_bias(7, 2) == 7);
static_assert(truncate.shr_bias(-7, 2) == -7);
static_assert(truncate.shr_bias(7u, 2) == 7u);

// shr_carry: pass-through
static_assert(truncate.shr_carry(3, 7, 1) == 3);
static_assert(truncate.shr_carry(-2, -3, 1) == -2);
static_assert(truncate.shr_carry(3u, 7u, 1) == 3u);

// composed shr: matches c++ default
static_assert(shr(0, 2, truncate) == 0);
static_assert(shr(4, 1, truncate) == 2);   // exact
static_assert(shr(-4, 1, truncate) == -2); // exact
static_assert(shr(3, 1, truncate) == 1);   // inexact, floors toward -inf
static_assert(shr(-3, 1, truncate) == -2); // inexact, floors toward -inf
static_assert(shr(3u, 1, truncate) == 1u);

// div_bias: pass-through
static_assert(truncate.div_bias(7u, 3u) == 7u);

// div_carry: pass-through
static_assert(truncate.div_carry(2u, 3u, 1u) == 2u);

// composed div: matches c++ default
static_assert(div(0u, 7u, truncate) == 0u);
static_assert(div(5u, 1u, truncate) == 5u); // exact
static_assert(div(3u, 2u, truncate) == 1u); // inexact

// div_shr: pass-through
static_assert(div_shr(3u, 2u, 0, truncate) == 1u); // shift == 0
static_assert(div_shr(6u, 3u, 1, truncate) == 1u); // remainder == 0
static_assert(div_shr(7u, 3u, 1, truncate) == 1u); // remainder != 0

// --------------------------------------------------------------------------------------------------------------------
// Mixed-Width Boundaries
// --------------------------------------------------------------------------------------------------------------------

// wide / narrow unsigned: quotient exceeds 32-bit max
static_assert(truncate.div_carry(static_cast<uint64_t>(0x1FFFFFFFFull), // quotient (odd, requires carry)
                                 static_cast<uint32_t>(2),              // divisor
                                 static_cast<uint32_t>(1))              // remainder
              == 0x1FFFFFFFFull);                                       // cleanly rolls over into the 34th bit

// wide / narrow signed: negative quotient exceeds 32-bit min
static_assert(truncate.div_carry(static_cast<int64_t>(-0x1FFFFFFFFll), // quotient (odd, requires carry)
                                 static_cast<int32_t>(2),              // divisor
                                 static_cast<int32_t>(-1))             // remainder
              == -0x1FFFFFFFFll); // carries more negative, preserving sign and width

// div_shr: routing with mixed types
static_assert(truncate.div_shr(static_cast<int64_t>(0x100000000ll), // shifted_quotient
                               static_cast<int64_t>(0x200000001ll), // quotient
                               static_cast<int32_t>(3),             // divisor
                               static_cast<int32_t>(2),             // remainder (routes to nearest_away)
                               1)                                   // shift
              == 0x100000000ll);                                    // carries successfully in wide_t space

// ====================================================================================================================
// nearest_up_t
// ====================================================================================================================

static_assert(preserves_type(nearest_up));

// shr_bias: pass-through
static_assert(nearest_up.shr_bias(7, 2) == 7);
static_assert(nearest_up.shr_bias(7u, 2) == 7u);

// --------------------------------------------------------------------------------------------------------------------
// shr_carry
// --------------------------------------------------------------------------------------------------------------------

// zero
static_assert(shr_carry(0, 2, nearest_up) == 0);

// exact
static_assert(shr_carry(4, 1, nearest_up) == 2);
static_assert(shr_carry(-4, 1, nearest_up) == -2);

// inexact: below half -> no carry
static_assert(shr_carry(1, 2, nearest_up) == 0);
static_assert(shr_carry(-1, 2, nearest_up) == 0);

// inexact: above half -> carry
static_assert(shr_carry(3, 2, nearest_up) == 1);
static_assert(shr_carry(-3, 2, nearest_up) == -1);

// ties: toward +infinity
static_assert(shr_carry(3, 1, nearest_up) == 2);   // positive tie -> up
static_assert(shr_carry(-3, 1, nearest_up) == -1); // negative tie -> up (toward +inf, i.e. less negative)
static_assert(shr_carry(-1, 1, nearest_up) == 0);  // negative tie -> up
static_assert(shr_carry(2, 2, nearest_up) == 1);   // tie -> up
static_assert(shr_carry(-2, 2, nearest_up) == 0);  // tie -> up

// unsigned
static_assert(shr_carry(1u, 2, nearest_up) == 0u);
static_assert(shr_carry(3u, 1, nearest_up) == 2u); // tie -> up
static_assert(shr_carry(2u, 2, nearest_up) == 1u); // tie -> up
static_assert(shr_carry(3u, 2, nearest_up) == 1u); // above half

// boundaries
static_assert(shr_carry<uint32_t>(0x80007FFFu, 16, nearest_up) == 0x8000u);    // below half
static_assert(shr_carry<uint32_t>(0xFFFFFFFFu, 1, nearest_up) == 0x80000000u); // tie -> up
static_assert(shr_carry<uint32_t>(0x80008000u, 16, nearest_up) == 0x8001u);    // tie -> up

// div_bias: pass-through
static_assert(nearest_up.div_bias(7u, 3u) == 7u);

// --------------------------------------------------------------------------------------------------------------------
// div_carry
// --------------------------------------------------------------------------------------------------------------------

// zero
static_assert(div_carry(0u, 7u, nearest_up) == 0u);

// exact
static_assert(div_carry(5u, 1u, nearest_up) == 5u);

// inexact: below half
static_assert(div_carry(1u, 4u, nearest_up) == 0u); // 1/4, rem 1 < 4/2

// inexact: above half
static_assert(div_carry(3u, 4u, nearest_up) == 1u); // 3/4, rem 3 > 4/2

// ties: (remainder + 1) > (divisor - remainder) when 2*rem >= divisor
static_assert(div_carry(1u, 2u, nearest_up) == 1u); // rem=1, (1+1)>(2-1) -> carry
static_assert(div_carry(3u, 2u, nearest_up) == 2u); // tie -> up
static_assert(div_carry(2u, 4u, nearest_up) == 1u); // tie, even divisor -> up

// odd divisor: no exact tie exists
static_assert(div_carry(1u, 3u, nearest_up) == 0u); // 1/3 < 0.5
static_assert(div_carry(2u, 3u, nearest_up) == 1u); // 2/3 > 0.5

// boundaries
static_assert(div_carry<uint32_t>(2147483647u, 4294967295u, nearest_up) == 0u); // just below half
static_assert(div_carry<uint32_t>(2147483648u, 4294967295u, nearest_up) == 1u); // just above half

// --------------------------------------------------------------------------------------------------------------------
// div_shr
// --------------------------------------------------------------------------------------------------------------------

// shift == 0: delegates to div_carry
static_assert(div_shr(1u, 2u, 0, nearest_up) == 1u); // tie -> up
static_assert(div_shr(3u, 2u, 0, nearest_up) == 2u);

// remainder == 0: delegates to shr_carry
static_assert(div_shr(6u, 2u, 1, nearest_up) == 2u); // 6/2=3, 3>>1=1, tie -> up = 2
static_assert(div_shr(6u, 3u, 1, nearest_up) == 1u); // 6/3=2, 2>>1=1, exact

// remainder != 0: delegates to nearest_away.shr_carry
static_assert(div_shr(7u, 3u, 1, nearest_up) == 1u); // 7/3=2 rem 1, 2>>1=1, frac=0 < half -> no carry
static_assert(div_shr(5u, 2u, 1, nearest_up) == 1u); // 5/2=2 rem 1, 2>>1=1, frac=0 -> 1

// boundaries
static_assert(div_shr<uint32_t>(0xFFFFFFFFu, 2u, 1, nearest_up) == 0x40000000u); // tie -> away -> up

// --------------------------------------------------------------------------------------------------------------------
// Mixed-Width Boundaries
// --------------------------------------------------------------------------------------------------------------------

// wide / narrow unsigned: quotient exceeds 32-bit max
static_assert(nearest_up.div_carry(static_cast<uint64_t>(0x1FFFFFFFFull), // quotient (odd, requires carry)
                                   static_cast<uint32_t>(2),              // divisor
                                   static_cast<uint32_t>(1))              // remainder
              == 0x200000000ull);                                         // cleanly rolls over into the 34th bit

// wide / narrow signed: negative quotient exceeds 32-bit min
static_assert(nearest_up.div_carry(static_cast<int64_t>(-0x1FFFFFFFFll), // quotient (odd, requires carry)
                                   static_cast<int32_t>(2),              // divisor
                                   static_cast<int32_t>(-1))             // remainder
              == -0x1FFFFFFFFll); // carries more negative, preserving sign and width

// div_shr: routing with mixed types
static_assert(nearest_up.div_shr(static_cast<int64_t>(0x100000000ll), // shifted_quotient
                                 static_cast<int64_t>(0x200000001ll), // quotient
                                 static_cast<int32_t>(3),             // divisor
                                 static_cast<int32_t>(2),             // remainder (routes to nearest_away)
                                 1)                                   // shift
              == 0x100000001ll);                                      // carries successfully in wide_t space

// ====================================================================================================================
// nearest_away_t
// ====================================================================================================================

// nearest_away inherits nearest_up's unsigned shr_carry, both shr_bias, and all div methods.
// The only override is the signed shr_carry, which rounds ties away from zero.

static_assert(preserves_type(nearest_away));

// --------------------------------------------------------------------------------------------------------------------
// shr_carry
// --------------------------------------------------------------------------------------------------------------------

// positive ties: same as nearest_up (carry)
static_assert(shr_carry(3, 1, nearest_away) == 2);
static_assert(shr_carry(2, 2, nearest_away) == 1);

// negative ties: away from zero = more negative (no carry, floor is already away)
static_assert(shr_carry(-3, 1, nearest_away) == -2); // tie -> away = -2 (differs from nearest_up's -1)
static_assert(shr_carry(-1, 1, nearest_away) == -1); // tie -> away = -1 (differs from nearest_up's 0)
static_assert(shr_carry(-2, 2, nearest_away) == -1); // tie -> away = -1 (differs from nearest_up's 0)

// inexact: same as nearest_up
static_assert(shr_carry(1, 2, nearest_away) == 0);
static_assert(shr_carry(-1, 2, nearest_away) == 0);
static_assert(shr_carry(3, 2, nearest_away) == 1);
static_assert(shr_carry(-3, 2, nearest_away) == -1);

// unsigned: inherited from nearest_up, identical
static_assert(shr_carry(3u, 1, nearest_away) == 2u);
static_assert(shr_carry(2u, 2, nearest_away) == 1u);

// boundaries
static_assert(shr_carry<uint32_t>(0x80007FFFu, 16, nearest_away) == 0x8000u);    // below half
static_assert(shr_carry<uint32_t>(0xFFFFFFFFu, 1, nearest_away) == 0x80000000u); // tie -> away
static_assert(shr_carry<uint32_t>(0x80008000u, 16, nearest_away) == 0x8001u);    // tie -> away

// div: identical to nearest_up for unsigned
static_assert(div_carry(1u, 2u, nearest_away) == div_carry(1u, 2u, nearest_up));
static_assert(div_carry(3u, 2u, nearest_away) == div_carry(3u, 2u, nearest_up));
static_assert(div_carry(1u, 4u, nearest_away) == div_carry(1u, 4u, nearest_up));
static_assert(div_carry(3u, 4u, nearest_away) == div_carry(3u, 4u, nearest_up));

// --------------------------------------------------------------------------------------------------------------------
// div_shr
// --------------------------------------------------------------------------------------------------------------------

// shift == 0: delegates to div_carry
static_assert(div_shr(1u, 2u, 0, nearest_away) == 1u);

// shift != 0, remainder == 0: shr_carry (same as nearest_up on unsigned)
static_assert(div_shr(6u, 2u, 1, nearest_away) == 2u); // 6/2=3, tie -> away
static_assert(div_shr(6u, 3u, 1, nearest_away) == 1u); // 6/3=2, exact

// shift != 0, remainder != 0: still shr_carry — remainder is ignored
static_assert(div_shr(7u, 3u, 1, nearest_away) == 1u);

// boundaries
static_assert(div_shr<uint32_t>(0xFFFFFFFFu, 2u, 1, nearest_away) == 0x40000000u);

// --------------------------------------------------------------------------------------------------------------------
// Mixed-Width Boundaries
// --------------------------------------------------------------------------------------------------------------------

// wide / narrow signed: negative quotient exceeds 32-bit min
static_assert(nearest_away.div_carry(static_cast<int64_t>(-0x1FFFFFFFFll), // quotient (odd, requires carry)
                                     static_cast<int32_t>(2),              // divisor
                                     static_cast<int32_t>(-1))             // remainder
              == -0x200000000ll); // carries more negative, preserving sign and width

// div_shr: routing with mixed types
static_assert(nearest_away.div_shr(static_cast<int64_t>(0x100000000ll), // shifted_quotient
                                   static_cast<int64_t>(0x200000001ll), // quotient
                                   static_cast<int32_t>(3),             // divisor
                                   static_cast<int32_t>(2),             // remainder (routes to nearest_away)
                                   1)                                   // shift
              == 0x100000001ll);                                        // carries successfully in wide_t space

// ====================================================================================================================
// nearest_even_t
// ====================================================================================================================

static_assert(preserves_type(nearest_even));

// shr_bias: pass-through
static_assert(nearest_even.shr_bias(7, 2) == 7);
static_assert(nearest_even.shr_bias(7u, 2) == 7u);

// --------------------------------------------------------------------------------------------------------------------
// shr_carry
// --------------------------------------------------------------------------------------------------------------------

// zero
static_assert(shr_carry(0, 2, nearest_even) == 0);

// exact
static_assert(shr_carry(-7, 1, nearest_even) == -4); // shifted = -4, even -> keep

// inexact: below half -> no carry, above half -> carry
static_assert(shr_carry(1, 2, nearest_even) == 0);
static_assert(shr_carry(3, 2, nearest_even) == 1);

// ties: round to nearest even (based on shifted parity)
static_assert(shr_carry(1, 1, nearest_even) == 0); // shifted=0 even -> keep
static_assert(shr_carry(3, 1, nearest_even) == 2); // shifted=1 odd  -> round
static_assert(shr_carry(5, 1, nearest_even) == 2); // shifted=2 even -> keep
static_assert(shr_carry(2, 2, nearest_even) == 0); // shifted=0 even -> keep
static_assert(shr_carry(6, 2, nearest_even) == 2); // shifted=1 odd  -> round

// signed ties
static_assert(shr_carry(-1, 1, nearest_even) == 0);   // shifted=-1 odd  -> round (toward 0)
static_assert(shr_carry(-3, 1, nearest_even) == -2);  // shifted=-2 even -> keep
static_assert(shr_carry(-2, 2, nearest_even) == 0);   // shifted=-1 odd  -> round
static_assert(shr_carry(-6, 2, nearest_even) == -2);  // shifted=-2 even -> keep
static_assert(shr_carry(-10, 2, nearest_even) == -2); // shifted=-3 odd  -> round

// unsigned ties
static_assert(shr_carry(1u, 1, nearest_even) == 0u); // shifted=0 even -> keep
static_assert(shr_carry(3u, 1, nearest_even) == 2u); // shifted=1 odd  -> round
static_assert(shr_carry(5u, 1, nearest_even) == 2u); // shifted=2 even -> keep
static_assert(shr_carry(7u, 1, nearest_even) == 4u); // shifted=3 odd  -> round
static_assert(shr_carry(2u, 2, nearest_even) == 0u); // shifted=0 even -> keep
static_assert(shr_carry(6u, 2, nearest_even) == 2u); // shifted=1 odd  -> round

// boundaries
static_assert(shr_carry<uint32_t>(0xFFFFFFFEu, 1, nearest_even) == 0x7FFFFFFFu); // exact
static_assert(shr_carry<uint32_t>(0xFFFFFFFDu, 1, nearest_even) == 0x7FFFFFFEu); // tie, shifted even -> keep
static_assert(shr_carry<uint32_t>(0xFFFFFFFFu, 1, nearest_even) == 0x80000000u); // tie, shifted odd  -> round
static_assert(shr_carry<uint32_t>(0x80008000u, 16, nearest_even) == 0x8000u);    // tie, shifted even -> keep
static_assert(shr_carry<uint32_t>(0x80018000u, 16, nearest_even) == 0x8002u);    // tie, shifted odd  -> round

// div_bias: pass-through
static_assert(nearest_even.div_bias(7u, 3u) == 7u);

// --------------------------------------------------------------------------------------------------------------------
// div_carry
// --------------------------------------------------------------------------------------------------------------------

// zero
static_assert(div_carry(0u, 7u, nearest_even) == 0u);

// exact
static_assert(div_carry(5u, 1u, nearest_even) == 5u);

// inexact: below half / above half
static_assert(div_carry(1u, 4u, nearest_even) == 0u);
static_assert(div_carry(3u, 4u, nearest_even) == 1u);

// odd divisor: no exact tie
static_assert(div_carry(1u, 3u, nearest_even) == 0u); // below half
static_assert(div_carry(2u, 3u, nearest_even) == 1u); // above half

// ties: round based on quotient parity
static_assert(div_carry(1u, 2u, nearest_even) == 0u); // q=0 even -> keep
static_assert(div_carry(3u, 2u, nearest_even) == 2u); // q=1 odd  -> round
static_assert(div_carry(5u, 2u, nearest_even) == 2u); // q=2 even -> keep
static_assert(div_carry(7u, 2u, nearest_even) == 4u); // q=3 odd  -> round
static_assert(div_carry(2u, 4u, nearest_even) == 0u); // q=0 even -> keep
static_assert(div_carry(6u, 4u, nearest_even) == 2u); // q=1 odd  -> round

// boundaries
static_assert(div_carry<uint32_t>(2147483647u, 4294967295u, nearest_even) == 0u); // below half, q=0 even
static_assert(div_carry<uint32_t>(2147483648u, 4294967295u, nearest_even) == 1u); // above half
static_assert(div_carry<uint32_t>(2147483647u, 4294967294u, nearest_even) == 0u); // tie, q=0 even -> keep
static_assert(div_carry<uint32_t>(3221225471u, 2147483647u, nearest_even) == 2u); // above tie, odd q -> round

// --------------------------------------------------------------------------------------------------------------------
// div_shr
// --------------------------------------------------------------------------------------------------------------------

// shift == 0: delegates to div_carry
static_assert(div_shr(1u, 2u, 0, nearest_even) == 0u); // tie, q=0 even -> keep
static_assert(div_shr(3u, 2u, 0, nearest_even) == 2u); // tie, q=1 odd  -> round

// remainder == 0: delegates to shr_carry (parity-based)
static_assert(div_shr(10u, 2u, 1, nearest_even) == 2u); // tie, shifted=2 even -> keep
static_assert(div_shr(6u, 2u, 1, nearest_even) == 2u);  // tie, shifted=1 odd  -> round

// remainder != 0: delegates to nearest_away.shr_carry (remainder breaks ties away)
static_assert(div_shr(3u, 2u, 1, nearest_even) == 1u); // tie -> away
static_assert(div_shr(5u, 2u, 1, nearest_even) == 1u); // exact after shift

// boundaries
static_assert(div_shr<uint32_t>(0xFFFFFFFEu, 2u, 1, nearest_even) == 0x40000000u); // tie, rem=0, odd  -> round
static_assert(div_shr<uint32_t>(0xFFFFFFFFu, 2u, 1, nearest_even) == 0x40000000u); // tie, rem!=0, away -> round

// --------------------------------------------------------------------------------------------------------------------
// Mixed-Width Boundaries
// --------------------------------------------------------------------------------------------------------------------

// wide / narrow unsigned: quotient exceeds 32-bit max
static_assert(nearest_even.div_carry(static_cast<uint64_t>(0x1FFFFFFFFull), // quotient (odd, requires carry)
                                     static_cast<uint32_t>(2),              // divisor
                                     static_cast<uint32_t>(1))              // remainder
              == 0x200000000ull);                                           // cleanly rolls over into the 34th bit

// wide / narrow signed: negative quotient exceeds 32-bit min
static_assert(nearest_even.div_carry(static_cast<int64_t>(-0x1FFFFFFFFll), // quotient (odd, requires carry)
                                     static_cast<int32_t>(2),              // divisor
                                     static_cast<int32_t>(-1))             // remainder
              == -0x200000000ll); // carries more negative, preserving sign and width

// div_shr: routing with mixed types
static_assert(nearest_even.div_shr(static_cast<int64_t>(0x100000000ll), // shifted_quotient
                                   static_cast<int64_t>(0x200000001ll), // quotient
                                   static_cast<int32_t>(3),             // divisor
                                   static_cast<int32_t>(2),             // remainder (routes to nearest_away)
                                   1)                                   // shift
              == 0x100000001ll);                                        // carries successfully in wide_t space

// ====================================================================================================================
// fast::nearest_up_t
// ====================================================================================================================

static_assert(preserves_type(fast::nearest_up));

// shr_bias: adds half
static_assert(fast::nearest_up.shr_bias(0, 1) == 1);    // 0 + (1 << 0) = 1
static_assert(fast::nearest_up.shr_bias(3, 1) == 4);    // 3 + 1 = 4
static_assert(fast::nearest_up.shr_bias(-3, 1) == -2);  // -3 + 1 = -2
static_assert(fast::nearest_up.shr_bias(0u, 2) == 2u);  // 0 + (1 << 1) = 2
static_assert(fast::nearest_up.shr_bias(7u, 3) == 11u); // 7 + 4 = 11

// shr_carry: pass-through
static_assert(fast::nearest_up.shr_carry(99, 0, 1) == 99);
static_assert(fast::nearest_up.shr_carry(99u, 0u, 1) == 99u);

// composed shr: must match safe nearest_up
static_assert(shr(0, 2, fast::nearest_up) == shr(0, 2, nearest_up));
static_assert(shr(3, 1, fast::nearest_up) == shr(3, 1, nearest_up));   // tie
static_assert(shr(-3, 1, fast::nearest_up) == shr(-3, 1, nearest_up)); // negative tie
static_assert(shr(-1, 1, fast::nearest_up) == shr(-1, 1, nearest_up)); // negative tie
static_assert(shr(1, 2, fast::nearest_up) == shr(1, 2, nearest_up));   // below half
static_assert(shr(3, 2, fast::nearest_up) == shr(3, 2, nearest_up));   // above half
static_assert(shr(3u, 1, fast::nearest_up) == shr(3u, 1, nearest_up)); // unsigned tie
static_assert(shr(2u, 2, fast::nearest_up) == shr(2u, 2, nearest_up)); // unsigned tie

// div_shr: inherited from nearest_up_t, uses safe shr_carry and div_carry
// This is the critical seam: nearest_up_t::div_shr calls nearest_up_t::shr_carry (safe),
// not fast::nearest_up_t::shr_carry (pass-through). Cross-check all 3 branches.
static_assert(div_shr(1u, 2u, 0, fast::nearest_up) == div_shr(1u, 2u, 0, nearest_up)); // !shift
static_assert(div_shr(6u, 2u, 1, fast::nearest_up) == div_shr(6u, 2u, 1, nearest_up)); // !remainder
static_assert(div_shr(7u, 3u, 1, fast::nearest_up) == div_shr(7u, 3u, 1, nearest_up)); // both nonzero

// ====================================================================================================================
// fast::nearest_away
// ====================================================================================================================

static_assert(preserves_type(fast::nearest_away));

// shr_bias unsigned: adds half
static_assert(fast::nearest_away.shr_bias(0u, 1) == 1u);  // 0 + 1 = 1
static_assert(fast::nearest_away.shr_bias(3u, 1) == 4u);  // 3 + 1 = 4
static_assert(fast::nearest_away.shr_bias(7u, 3) == 11u); // 7 + 4 = 11

// shr_bias signed: half for positive, half - 1 for negative
static_assert(fast::nearest_away.shr_bias(3, 1) == 4);   // positive: 3 + 1 = 4
static_assert(fast::nearest_away.shr_bias(0, 1) == 1);   // zero is non-negative: 0 + 1 = 1
static_assert(fast::nearest_away.shr_bias(-3, 1) == -3); // negative: -3 + (1 - 1) = -3
static_assert(fast::nearest_away.shr_bias(-1, 1) == -1); // negative: -1 + 0 = -1
static_assert(fast::nearest_away.shr_bias(-4, 2) == -3); // negative: -4 + (2 - 1) = -3

// shr_carry: pass-through
static_assert(fast::nearest_away.shr_carry(99, 0, 1) == 99);
static_assert(fast::nearest_away.shr_carry(99u, 0u, 1) == 99u);

// composed shr: must match safe nearest_away
static_assert(shr(3, 1, fast::nearest_away) == shr(3, 1, nearest_away));   // positive tie
static_assert(shr(-3, 1, fast::nearest_away) == shr(-3, 1, nearest_away)); // negative tie
static_assert(shr(-1, 1, fast::nearest_away) == shr(-1, 1, nearest_away)); // negative tie
static_assert(shr(1, 2, fast::nearest_away) == shr(1, 2, nearest_away));   // below half
static_assert(shr(3, 2, fast::nearest_away) == shr(3, 2, nearest_away));   // above half
static_assert(shr(-1, 2, fast::nearest_away) == shr(-1, 2, nearest_away)); // below half, negative
static_assert(shr(-3, 2, fast::nearest_away) == shr(-3, 2, nearest_away)); // above half, negative
static_assert(shr(3u, 1, fast::nearest_away) == shr(3u, 1, nearest_away)); // unsigned tie
static_assert(shr(2u, 2, fast::nearest_away) == shr(2u, 2, nearest_away)); // unsigned tie

// div_bias: adds floor(divisor / 2)
static_assert(fast::nearest_away.div_bias(0u, 4u) == 2u); // 0 + 4/2 = 2
static_assert(fast::nearest_away.div_bias(1u, 4u) == 3u); // 1 + 2 = 3
static_assert(fast::nearest_away.div_bias(1u, 2u) == 2u); // 1 + 1 = 2
static_assert(fast::nearest_away.div_bias(0u, 3u) == 1u); // 0 + 3/2 = 1 (floor)
static_assert(fast::nearest_away.div_bias(1u, 3u) == 2u); // 1 + 1 = 2

// div_carry: pass-through
static_assert(fast::nearest_away.div_carry(99u, 3u, 1u) == 99u);

// composed div: must match safe nearest_away on unsigned
static_assert(div(1u, 2u, fast::nearest_away) == div(1u, 2u, nearest_away)); // tie
static_assert(div(1u, 4u, fast::nearest_away) == div(1u, 4u, nearest_away)); // below half
static_assert(div(3u, 4u, fast::nearest_away) == div(3u, 4u, nearest_away)); // above half
static_assert(div(2u, 3u, fast::nearest_away) == div(2u, 3u, nearest_away)); // above half, odd divisor

// div_shr: inherited from nearest_away_t, uses safe shr_carry and div_carry
// This is the critical seam: nearest_away_t::div_shr calls nearest_away_t::shr_carry (safe),
// not fast::nearest_away_t::shr_carry (pass-through). Cross-check all 3 branches.
static_assert(div_shr(1u, 2u, 0, fast::nearest_away) == div_shr(1u, 2u, 0, nearest_away)); // !shift
static_assert(div_shr(6u, 2u, 1, fast::nearest_away) == div_shr(6u, 2u, 1, nearest_away)); // !remainder
static_assert(div_shr(7u, 3u, 1, fast::nearest_away) == div_shr(7u, 3u, 1, nearest_away)); // both nonzero

// --------------------------------------------------------------------------------------------------------------------
// Mixed-Width Boundaries
// --------------------------------------------------------------------------------------------------------------------

// delegates div_bias to safe nearest_up_t (which is a pass-through)
static_assert(fast::nearest_up.div_bias(static_cast<uint64_t>(100), // dividend
                                        static_cast<uint32_t>(5))   // divisor
              == 100ull);                                           // unchanged

// delegates div_carry to safe nearest_up_t (which actually executes the carry)
static_assert(fast::nearest_up.div_carry(static_cast<uint64_t>(42), // quotient
                                         static_cast<uint32_t>(5),  // divisor
                                         static_cast<uint32_t>(3))  // remainder
              == 43ull);                                            // (3 + 1) > (5 - 3) -> carry

} // namespace
} // namespace rounding_modes
} // namespace crv
