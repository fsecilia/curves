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

// is_shr_rounding_mode: single value type
static_assert(is_shr_rounding_mode<shr::nearest_up_t, int_t>);
static_assert(is_shr_rounding_mode<shr::nearest_up_t, uint_t>);
static_assert(is_shr_rounding_mode<shr::nearest_away_t, int_t>);
static_assert(is_shr_rounding_mode<shr::nearest_even_t, int_t>);
static_assert(is_shr_rounding_mode<shr::truncate_t, int_t>);
static_assert(!is_shr_rounding_mode<arbitrary_t, int_t>);

// is_div_rounding_mode: wide and narrow types, sign must match
static_assert(is_div_rounding_mode<div::nearest_away_t, uint_t, uint_t>);
static_assert(is_div_rounding_mode<div::nearest_away_t, uint32_t, uint16_t>);
static_assert(is_div_rounding_mode<div::nearest_even_t, uint_t, uint_t>);
static_assert(is_div_rounding_mode<div::truncate_t, uint_t, uint_t>);
static_assert(!is_div_rounding_mode<div::nearest_away_t, int_t, int_t>);
static_assert(!is_div_rounding_mode<div::nearest_away_t, int_t, uint_t>);
static_assert(!is_div_rounding_mode<div::nearest_away_t, uint_t, int_t>);
static_assert(!is_div_rounding_mode<arbitrary_t, uint_t, uint_t>);

// ====================================================================================================================
// Rounding Modes Support
// ====================================================================================================================

/// calls shr_carry directly with the arithmetic-shifted value
template <typename value_t, typename sut_t>
constexpr auto shr_carry(value_t unshifted, int_t shift, sut_t sut) -> value_t
{
    return sut.carry(static_cast<value_t>(unshifted >> shift), unshifted, shift);
}

/// composed shr: bias -> shift -> carry
template <typename value_t, typename sut_t> constexpr auto shr(value_t unshifted, int_t shift, sut_t sut) -> value_t
{
    auto const biased  = sut.bias(unshifted, shift);
    auto const shifted = static_cast<value_t>(biased >> shift);
    return sut.carry(shifted, biased, shift);
}

/// calls div_carry directly with the truncated quotient and remainder
template <typename value_t, typename sut_t>
constexpr auto div_carry(value_t dividend, value_t divisor, sut_t sut) -> value_t
{
    return sut.carry(static_cast<value_t>(dividend / divisor), divisor, static_cast<value_t>(dividend % divisor));
}

/// composed div: bias -> divide -> carry
template <typename value_t, typename sut_t> constexpr auto div(value_t dividend, value_t divisor, sut_t sut) -> value_t
{
    auto const biased    = sut.bias(dividend, divisor);
    auto const quotient  = static_cast<value_t>(biased / divisor);
    auto const remainder = static_cast<value_t>(biased % divisor);
    return sut.carry(quotient, divisor, remainder);
}

/// Tests that shr apis return the parameterized type, even in the face of small integer promotion rules.
struct preserves_shr_type_t
{
    constexpr auto operator()(auto rounding_mode) const noexcept -> bool
    {
        return std::same_as<uint8_t, decltype(rounding_mode.bias(std::declval<uint8_t>(), std::declval<int_t>()))>
               && std::same_as<uint8_t, decltype(rounding_mode.carry(std::declval<uint8_t>(), std::declval<uint8_t>(),
                                                                     std::declval<int_t>()))>;
    }
};
constexpr auto preserves_shr_type = preserves_shr_type_t{};

/// Tests that div apis return the parameterized type, even in the face of small integer promotion rules.
struct preserves_div_type_t
{
    constexpr auto operator()(auto rounding_mode) const noexcept -> bool
    {
        return std::same_as<uint8_t, decltype(rounding_mode.bias(std::declval<uint8_t>(), std::declval<uint8_t>()))>
               && std::same_as<uint8_t, decltype(rounding_mode.carry(std::declval<uint8_t>(), std::declval<uint8_t>(),
                                                                     std::declval<uint8_t>()))>;
    }
};
constexpr auto preserves_div_type = preserves_div_type_t{};

// ====================================================================================================================
// shr::truncate_t
// ====================================================================================================================

static_assert(preserves_shr_type(shr::truncate));

// bias: pass-through
static_assert(shr::truncate.bias(7, 2) == 7);
static_assert(shr::truncate.bias(-7, 2) == -7);
static_assert(shr::truncate.bias(7u, 2) == 7u);

// carry: pass-through
static_assert(shr::truncate.carry(3, 7, 1) == 3);
static_assert(shr::truncate.carry(-2, -3, 1) == -2);
static_assert(shr::truncate.carry(3u, 7u, 1) == 3u);

// composed: matches c++ default
static_assert(shr(0, 2, shr::truncate) == 0);
static_assert(shr(4, 1, shr::truncate) == 2);   // exact
static_assert(shr(-4, 1, shr::truncate) == -2); // exact
static_assert(shr(3, 1, shr::truncate) == 1);   // inexact, floors toward -inf
static_assert(shr(-3, 1, shr::truncate) == -2); // inexact, floors toward -inf
static_assert(shr(3u, 1, shr::truncate) == 1u);

// ====================================================================================================================
// shr::nearest_up_t
// ====================================================================================================================

static_assert(preserves_shr_type(shr::nearest_up));

// bias: pass-through
static_assert(shr::nearest_up.bias(7, 2) == 7);
static_assert(shr::nearest_up.bias(7u, 2) == 7u);

// --------------------------------------------------------------------------------------------------------------------
// carry
// --------------------------------------------------------------------------------------------------------------------

// zero
static_assert(shr_carry(0, 2, shr::nearest_up) == 0);

// exact
static_assert(shr_carry(4, 1, shr::nearest_up) == 2);
static_assert(shr_carry(-4, 1, shr::nearest_up) == -2);

// inexact: below half -> no carry
static_assert(shr_carry(1, 2, shr::nearest_up) == 0);
static_assert(shr_carry(-1, 2, shr::nearest_up) == 0);

// inexact: above half -> carry
static_assert(shr_carry(3, 2, shr::nearest_up) == 1);
static_assert(shr_carry(-3, 2, shr::nearest_up) == -1);

// ties: toward +infinity
static_assert(shr_carry(3, 1, shr::nearest_up) == 2);   // positive tie -> up
static_assert(shr_carry(-3, 1, shr::nearest_up) == -1); // negative tie -> up (toward +inf, i.e. less negative)
static_assert(shr_carry(-1, 1, shr::nearest_up) == 0);  // negative tie -> up
static_assert(shr_carry(2, 2, shr::nearest_up) == 1);   // tie -> up
static_assert(shr_carry(-2, 2, shr::nearest_up) == 0);  // tie -> up

// unsigned
static_assert(shr_carry(1u, 2, shr::nearest_up) == 0u);
static_assert(shr_carry(3u, 1, shr::nearest_up) == 2u); // tie -> up
static_assert(shr_carry(2u, 2, shr::nearest_up) == 1u); // tie -> up
static_assert(shr_carry(3u, 2, shr::nearest_up) == 1u); // above half

// boundaries
static_assert(shr_carry<uint32_t>(0x80007FFFu, 16, shr::nearest_up) == 0x8000u);    // below half
static_assert(shr_carry<uint32_t>(0xFFFFFFFFu, 1, shr::nearest_up) == 0x80000000u); // tie -> up
static_assert(shr_carry<uint32_t>(0x80008000u, 16, shr::nearest_up) == 0x8001u);    // tie -> up

// ====================================================================================================================
// shr::nearest_away_t
//
// shr::nearest_away_t inherits nearest_up_t's unsigned carry and both bias methods. The only override is signed carry,
// which rounds ties away from zero.
// ====================================================================================================================

static_assert(preserves_shr_type(shr::nearest_away));

// --------------------------------------------------------------------------------------------------------------------
// carry
// --------------------------------------------------------------------------------------------------------------------

// positive ties: same as nearest_up (carry)
static_assert(shr_carry(3, 1, shr::nearest_away) == 2);
static_assert(shr_carry(2, 2, shr::nearest_away) == 1);

// negative ties: away from zero = more negative (no carry, floor is already away)
static_assert(shr_carry(-3, 1, shr::nearest_away) == -2); // tie -> away = -2 (differs from nearest_up's -1)
static_assert(shr_carry(-1, 1, shr::nearest_away) == -1); // tie -> away = -1 (differs from nearest_up's 0)
static_assert(shr_carry(-2, 2, shr::nearest_away) == -1); // tie -> away = -1 (differs from nearest_up's 0)

// inexact: same as nearest_up
static_assert(shr_carry(1, 2, shr::nearest_away) == 0);
static_assert(shr_carry(-1, 2, shr::nearest_away) == 0);
static_assert(shr_carry(3, 2, shr::nearest_away) == 1);
static_assert(shr_carry(-3, 2, shr::nearest_away) == -1);

// unsigned: inherited from nearest_up, identical
static_assert(shr_carry(3u, 1, shr::nearest_away) == 2u);
static_assert(shr_carry(2u, 2, shr::nearest_away) == 1u);

// boundaries
static_assert(shr_carry<uint32_t>(0x80007FFFu, 16, shr::nearest_away) == 0x8000u);    // below half
static_assert(shr_carry<uint32_t>(0xFFFFFFFFu, 1, shr::nearest_away) == 0x80000000u); // tie -> away
static_assert(shr_carry<uint32_t>(0x80008000u, 16, shr::nearest_away) == 0x8001u);    // tie -> away

// ====================================================================================================================
// shr::nearest_even_t
// ====================================================================================================================

static_assert(preserves_shr_type(shr::nearest_even));

// shr_bias: pass-through
static_assert(shr::nearest_even.bias(7, 2) == 7);
static_assert(shr::nearest_even.bias(7u, 2) == 7u);

// --------------------------------------------------------------------------------------------------------------------
// shr_carry
// --------------------------------------------------------------------------------------------------------------------

// zero
static_assert(shr_carry(0, 2, shr::nearest_even) == 0);

// exact
static_assert(shr_carry(-7, 1, shr::nearest_even) == -4); // shifted = -4, even -> keep

// inexact: below half -> no carry, above half -> carry
static_assert(shr_carry(1, 2, shr::nearest_even) == 0);
static_assert(shr_carry(3, 2, shr::nearest_even) == 1);

// ties: round to nearest even (based on shifted parity)
static_assert(shr_carry(1, 1, shr::nearest_even) == 0); // shifted=0 even -> keep
static_assert(shr_carry(3, 1, shr::nearest_even) == 2); // shifted=1 odd  -> round
static_assert(shr_carry(5, 1, shr::nearest_even) == 2); // shifted=2 even -> keep
static_assert(shr_carry(2, 2, shr::nearest_even) == 0); // shifted=0 even -> keep
static_assert(shr_carry(6, 2, shr::nearest_even) == 2); // shifted=1 odd  -> round

// signed ties
static_assert(shr_carry(-1, 1, shr::nearest_even) == 0);   // shifted=-1 odd  -> round (toward 0)
static_assert(shr_carry(-3, 1, shr::nearest_even) == -2);  // shifted=-2 even -> keep
static_assert(shr_carry(-2, 2, shr::nearest_even) == 0);   // shifted=-1 odd  -> round
static_assert(shr_carry(-6, 2, shr::nearest_even) == -2);  // shifted=-2 even -> keep
static_assert(shr_carry(-10, 2, shr::nearest_even) == -2); // shifted=-3 odd  -> round

// unsigned ties
static_assert(shr_carry(1u, 1, shr::nearest_even) == 0u); // shifted=0 even -> keep
static_assert(shr_carry(3u, 1, shr::nearest_even) == 2u); // shifted=1 odd  -> round
static_assert(shr_carry(5u, 1, shr::nearest_even) == 2u); // shifted=2 even -> keep
static_assert(shr_carry(7u, 1, shr::nearest_even) == 4u); // shifted=3 odd  -> round
static_assert(shr_carry(2u, 2, shr::nearest_even) == 0u); // shifted=0 even -> keep
static_assert(shr_carry(6u, 2, shr::nearest_even) == 2u); // shifted=1 odd  -> round

// boundaries
static_assert(shr_carry<uint32_t>(0xFFFFFFFEu, 1, shr::nearest_even) == 0x7FFFFFFFu); // exact
static_assert(shr_carry<uint32_t>(0xFFFFFFFDu, 1, shr::nearest_even) == 0x7FFFFFFEu); // tie, shifted even -> keep
static_assert(shr_carry<uint32_t>(0xFFFFFFFFu, 1, shr::nearest_even) == 0x80000000u); // tie, shifted odd  -> round
static_assert(shr_carry<uint32_t>(0x80008000u, 16, shr::nearest_even) == 0x8000u);    // tie, shifted even -> keep
static_assert(shr_carry<uint32_t>(0x80018000u, 16, shr::nearest_even) == 0x8002u);    // tie, shifted odd  -> round

// ====================================================================================================================
// shr::fast::nearest_up_t
// ====================================================================================================================

static_assert(preserves_shr_type(shr::fast::nearest_up));

// shr_bias: adds half
static_assert(shr::fast::nearest_up.bias(0, 1) == 1);    // 0 + (1 << 0) = 1
static_assert(shr::fast::nearest_up.bias(3, 1) == 4);    // 3 + 1 = 4
static_assert(shr::fast::nearest_up.bias(-3, 1) == -2);  // -3 + 1 = -2
static_assert(shr::fast::nearest_up.bias(0u, 2) == 2u);  // 0 + (1 << 1) = 2
static_assert(shr::fast::nearest_up.bias(7u, 3) == 11u); // 7 + 4 = 11

// shr_carry: pass-through
static_assert(shr::fast::nearest_up.carry(99, 0, 1) == 99);
static_assert(shr::fast::nearest_up.carry(99u, 0u, 1) == 99u);

// composed shr: must match safe nearest_up
static_assert(shr(0, 2, shr::fast::nearest_up) == shr(0, 2, shr::nearest_up));
static_assert(shr(3, 1, shr::fast::nearest_up) == shr(3, 1, shr::nearest_up));   // tie
static_assert(shr(-3, 1, shr::fast::nearest_up) == shr(-3, 1, shr::nearest_up)); // negative tie
static_assert(shr(-1, 1, shr::fast::nearest_up) == shr(-1, 1, shr::nearest_up)); // negative tie
static_assert(shr(1, 2, shr::fast::nearest_up) == shr(1, 2, shr::nearest_up));   // below half
static_assert(shr(3, 2, shr::fast::nearest_up) == shr(3, 2, shr::nearest_up));   // above half
static_assert(shr(3u, 1, shr::fast::nearest_up) == shr(3u, 1, shr::nearest_up)); // unsigned tie
static_assert(shr(2u, 2, shr::fast::nearest_up) == shr(2u, 2, shr::nearest_up)); // unsigned tie

// ====================================================================================================================
// shr::fast::nearest_away_t
// ====================================================================================================================

static_assert(preserves_shr_type(shr::fast::nearest_away));

// shr_bias unsigned: adds half
static_assert(shr::fast::nearest_away.bias(0u, 1) == 1u);  // 0 + 1 = 1
static_assert(shr::fast::nearest_away.bias(3u, 1) == 4u);  // 3 + 1 = 4
static_assert(shr::fast::nearest_away.bias(7u, 3) == 11u); // 7 + 4 = 11

// shr_bias signed: half for positive, half - 1 for negative
static_assert(shr::fast::nearest_away.bias(3, 1) == 4);   // positive: 3 + 1 = 4
static_assert(shr::fast::nearest_away.bias(0, 1) == 1);   // zero is non-negative: 0 + 1 = 1
static_assert(shr::fast::nearest_away.bias(-3, 1) == -3); // negative: -3 + (1 - 1) = -3
static_assert(shr::fast::nearest_away.bias(-1, 1) == -1); // negative: -1 + 0 = -1
static_assert(shr::fast::nearest_away.bias(-4, 2) == -3); // negative: -4 + (2 - 1) = -3

// shr_carry: pass-through
static_assert(shr::fast::nearest_away.carry(99, 0, 1) == 99);
static_assert(shr::fast::nearest_away.carry(99u, 0u, 1) == 99u);

// composed shr: must match safe nearest_away
static_assert(shr(3, 1, shr::fast::nearest_away) == shr(3, 1, shr::nearest_away));   // positive tie
static_assert(shr(-3, 1, shr::fast::nearest_away) == shr(-3, 1, shr::nearest_away)); // negative tie
static_assert(shr(-1, 1, shr::fast::nearest_away) == shr(-1, 1, shr::nearest_away)); // negative tie
static_assert(shr(1, 2, shr::fast::nearest_away) == shr(1, 2, shr::nearest_away));   // below half
static_assert(shr(3, 2, shr::fast::nearest_away) == shr(3, 2, shr::nearest_away));   // above half
static_assert(shr(-1, 2, shr::fast::nearest_away) == shr(-1, 2, shr::nearest_away)); // below half, negative
static_assert(shr(-3, 2, shr::fast::nearest_away) == shr(-3, 2, shr::nearest_away)); // above half, negative
static_assert(shr(3u, 1, shr::fast::nearest_away) == shr(3u, 1, shr::nearest_away)); // unsigned tie
static_assert(shr(2u, 2, shr::fast::nearest_away) == shr(2u, 2, shr::nearest_away)); // unsigned tie

// ====================================================================================================================
// div::truncate_t
// ====================================================================================================================

static_assert(preserves_div_type(div::truncate));

// bias: pass-through
static_assert(div::truncate.bias(7u, 3u) == 7u);

// carry: pass-through
static_assert(div::truncate.carry(2u, 3u, 1u) == 2u);

// composed: matches c++ default
static_assert(div(0u, 7u, div::truncate) == 0u);
static_assert(div(5u, 1u, div::truncate) == 5u); // exact
static_assert(div(3u, 2u, div::truncate) == 1u); // inexact

// wide / narrow: quotient exceeds 32-bit max
static_assert(div::truncate.carry(static_cast<uint64_t>(0x1FFFFFFFFull), // quotient (odd, requires carry)
                                  static_cast<uint32_t>(2),              // divisor
                                  static_cast<uint32_t>(1))              // remainder
              == 0x1FFFFFFFFull);                                        // cleanly rolls over into the 34th bit

// ====================================================================================================================
// div::nearest_away_t
// ====================================================================================================================

static_assert(preserves_div_type(div::nearest_away));

// div_bias: pass-through
static_assert(div::nearest_away.bias(7u, 3u) == 7u);

// --------------------------------------------------------------------------------------------------------------------
// div_carry
// --------------------------------------------------------------------------------------------------------------------

// zero
static_assert(div_carry(0u, 7u, div::nearest_away) == 0u);

// exact
static_assert(div_carry(5u, 1u, div::nearest_away) == 5u);

// inexact: below half
static_assert(div_carry(1u, 4u, div::nearest_away) == 0u); // 1/4, rem 1 < 4/2

// inexact: above half
static_assert(div_carry(3u, 4u, div::nearest_away) == 1u); // 3/4, rem 3 > 4/2

// ties: (remainder + 1) > (divisor - remainder) when 2*rem >= divisor
static_assert(div_carry(1u, 2u, div::nearest_away) == 1u); // rem=1, (1+1)>(2-1) -> carry
static_assert(div_carry(3u, 2u, div::nearest_away) == 2u); // tie -> up
static_assert(div_carry(2u, 4u, div::nearest_away) == 1u); // tie, even divisor -> up

// odd divisor: no exact tie exists
static_assert(div_carry(1u, 3u, div::nearest_away) == 0u); // 1/3 < 0.5
static_assert(div_carry(2u, 3u, div::nearest_away) == 1u); // 2/3 > 0.5

// boundaries
static_assert(div_carry<uint32_t>(2147483647u, 4294967295u, div::nearest_away) == 0u); // just below half
static_assert(div_carry<uint32_t>(2147483648u, 4294967295u, div::nearest_away) == 1u); // just above half

// composed div: matches carry
static_assert(div(0u, 7u, div::nearest_away) == 0u);
static_assert(div(5u, 1u, div::nearest_away) == 5u);
static_assert(div(3u, 2u, div::nearest_away) == 2u);

// ====================================================================================================================
// div::nearest_even_t
// ====================================================================================================================

static_assert(preserves_div_type(div::nearest_even));

// div_bias: pass-through
static_assert(div::nearest_even.bias(7u, 3u) == 7u);

// --------------------------------------------------------------------------------------------------------------------
// div_carry
// --------------------------------------------------------------------------------------------------------------------

// zero
static_assert(div_carry(0u, 7u, div::nearest_even) == 0u);

// exact
static_assert(div_carry(5u, 1u, div::nearest_even) == 5u);

// inexact: below half / above half
static_assert(div_carry(1u, 4u, div::nearest_even) == 0u);
static_assert(div_carry(3u, 4u, div::nearest_even) == 1u);

// odd divisor: no exact tie
static_assert(div_carry(1u, 3u, div::nearest_even) == 0u); // below half
static_assert(div_carry(2u, 3u, div::nearest_even) == 1u); // above half

// ties: round based on quotient parity
static_assert(div_carry(1u, 2u, div::nearest_even) == 0u); // q=0 even -> keep
static_assert(div_carry(3u, 2u, div::nearest_even) == 2u); // q=1 odd  -> round
static_assert(div_carry(5u, 2u, div::nearest_even) == 2u); // q=2 even -> keep
static_assert(div_carry(7u, 2u, div::nearest_even) == 4u); // q=3 odd  -> round
static_assert(div_carry(2u, 4u, div::nearest_even) == 0u); // q=0 even -> keep
static_assert(div_carry(6u, 4u, div::nearest_even) == 2u); // q=1 odd  -> round

// boundaries
static_assert(div_carry<uint32_t>(2147483647u, 4294967295u, div::nearest_even) == 0u); // below half, q=0 even
static_assert(div_carry<uint32_t>(2147483648u, 4294967295u, div::nearest_even) == 1u); // above half
static_assert(div_carry<uint32_t>(2147483647u, 4294967294u, div::nearest_even) == 0u); // tie, q=0 even -> keep
static_assert(div_carry<uint32_t>(3221225471u, 2147483647u, div::nearest_even) == 2u); // above tie, odd q -> round

// wide / narrow unsigned: quotient exceeds 32-bit max
static_assert(div::nearest_even.carry(static_cast<uint64_t>(0x1FFFFFFFFull), // quotient (odd, requires carry)
                                      static_cast<uint32_t>(2),              // divisor
                                      static_cast<uint32_t>(1))              // remainder
              == 0x200000000ull);                                            // cleanly rolls over into the 34th bit

// ====================================================================================================================
// div::fast::nearest_away_t
// ====================================================================================================================

// div_bias: adds floor(divisor / 2)
static_assert(div::fast::nearest_away.bias(0u, 4u) == 2u); // 0 + 4/2 = 2
static_assert(div::fast::nearest_away.bias(1u, 4u) == 3u); // 1 + 2 = 3
static_assert(div::fast::nearest_away.bias(1u, 2u) == 2u); // 1 + 1 = 2
static_assert(div::fast::nearest_away.bias(0u, 3u) == 1u); // 0 + 3/2 = 1 (floor)
static_assert(div::fast::nearest_away.bias(1u, 3u) == 2u); // 1 + 1 = 2

// div_carry: pass-through
static_assert(div::fast::nearest_away.carry(99u, 3u, 1u) == 99u);

// composed div: must match safe nearest_away on unsigned
static_assert(div(1u, 2u, div::fast::nearest_away) == div(1u, 2u, div::nearest_away)); // tie
static_assert(div(1u, 4u, div::fast::nearest_away) == div(1u, 4u, div::nearest_away)); // below half
static_assert(div(3u, 4u, div::fast::nearest_away) == div(3u, 4u, div::nearest_away)); // above half
static_assert(div(2u, 3u, div::fast::nearest_away) == div(2u, 3u, div::nearest_away)); // above half, odd divisor

} // namespace
} // namespace rounding_modes
} // namespace crv
