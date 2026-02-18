// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include <crv/math/limits.hpp>
#include <crv/math/rounding_modes.hpp>
#include <crv/test/test.hpp>

namespace crv::rounding_modes {
namespace {

template <typename rounding_mode_t, typename value_t> constexpr auto shr(value_t in, int_t shift) -> value_t
{
    return rounding_mode_t{}.shr(in >> shift, in, shift);
}

template <typename rounding_mode_t, typename value_t> constexpr auto div(value_t dividend, value_t divisor) -> value_t
{
    return rounding_mode_t{}.div(dividend / divisor, divisor, dividend % divisor);
}

template <typename rounding_mode_t, typename value_t>
constexpr auto div_shr(value_t dividend, value_t divisor, int_t shift) -> value_t
{
    auto const quotient         = dividend / divisor;
    auto const remainder        = dividend % divisor;
    auto const shifted_quotient = quotient >> shift;

    return rounding_mode_t{}.div_shr(shifted_quotient, quotient, divisor, remainder, shift);
}

// ====================================================================================================================
// Truncate
// ====================================================================================================================

// shr
// --------------------------------------------------------------------------------------------------------------------
static_assert(shr<truncate>(0, 1) == 0);
static_assert(shr<truncate>(4, 1) == 2);   // exact
static_assert(shr<truncate>(3, 1) == 1);   //  1.5 -> 1 (floor)
static_assert(shr<truncate>(-4, 1) == -2); // exact
static_assert(shr<truncate>(-3, 1) == -2); // -1.5 -> -2 (floor, not toward 0)
static_assert(shr<truncate>(0u, 1) == 0);
static_assert(shr<truncate>(3u, 1) == 1);

// div
// --------------------------------------------------------------------------------------------------------------------
static_assert(div<truncate>(0, 2) == 0);
static_assert(div<truncate>(3, 2) == 1);   //  1.5 ->  1 (toward 0)
static_assert(div<truncate>(-3, 2) == -1); // -1.5 -> -1 (toward 0, differs from shr)
static_assert(div<truncate>(2, 3) == 0);   //  0.67 -> 0
static_assert(div<truncate>(-2, 3) == 0);  // -0.67 -> 0
static_assert(div<truncate>(3u, 2u) == 1);
static_assert(div<truncate>(2u, 3u) == 0);

// div_shr
// --------------------------------------------------------------------------------------------------------------------
static_assert(div_shr<truncate>(3, 2, 0) == 1);   // shift=0 -> div:  1.5 ->  1
static_assert(div_shr<truncate>(-3, 2, 0) == -1); // shift=0 -> div: -1.5 -> -1
static_assert(div_shr<truncate>(6, 3, 1) == 1);   // rem=0 -> shr: q=2, shr 1 -> 1
static_assert(div_shr<truncate>(7, 3, 1) == 1);   // general: q=2, r=1, shr 1 -> 1
static_assert(div_shr<truncate>(-7, 3, 1) == -1); // general: q=-2, r=-1, shr 1 -> -1

// boundary
// --------------------------------------------------------------------------------------------------------------------
static_assert(shr<truncate>(0, 2) == 0);
static_assert(div<truncate>(0, 7) == 0);
static_assert(div<truncate>(5, 1) == 5);

// ====================================================================================================================
// Asymmetric
// ====================================================================================================================

// shr
// --------------------------------------------------------------------------------------------------------------------

// shift = 1: every inexact case is a tie
static_assert(shr<asymmetric>(4, 1) == 2);   // exact
static_assert(shr<asymmetric>(3, 1) == 2);   //  1.5 ->  2 (up)
static_assert(shr<asymmetric>(-3, 1) == -1); // -1.5 -> -1 (toward +inf)
static_assert(shr<asymmetric>(-1, 1) == 0);  // -0.5 ->  0 (toward +inf)

// shift = 2: below / at / above half, both signs
static_assert(shr<asymmetric>(1, 2) == 0);   //  0.25 -> 0 (below)
static_assert(shr<asymmetric>(2, 2) == 1);   //  0.5  -> 1 (tie, up)
static_assert(shr<asymmetric>(3, 2) == 1);   //  0.75 -> 1 (above)
static_assert(shr<asymmetric>(-1, 2) == 0);  // -0.25 -> 0
static_assert(shr<asymmetric>(-2, 2) == 0);  // -0.5  -> 0 (tie, toward +inf)
static_assert(shr<asymmetric>(-3, 2) == -1); // -0.75 -> -1

// unsigned
static_assert(shr<asymmetric>(3u, 1) == 2);
static_assert(shr<asymmetric>(1u, 2) == 0);
static_assert(shr<asymmetric>(2u, 2) == 1);
static_assert(shr<asymmetric>(3u, 2) == 1);

// div
// --------------------------------------------------------------------------------------------------------------------

// signed, even divisor
static_assert(div<asymmetric>(1, 2) == 1);  //  0.5 ->  1 (tie, up)
static_assert(div<asymmetric>(-1, 2) == 0); // -0.5 ->  0 (tie, toward +inf)
static_assert(div<asymmetric>(1, 4) == 0);  //  0.25 -> 0
static_assert(div<asymmetric>(3, 4) == 1);  //  0.75 -> 1
static_assert(div<asymmetric>(-2, 4) == 0); // -0.5  -> 0

// signed, odd divisor
static_assert(div<asymmetric>(1, 3) == 0);   //  0.33 -> 0
static_assert(div<asymmetric>(2, 3) == 1);   //  0.67 -> 1
static_assert(div<asymmetric>(-2, 3) == -1); // -0.67 -> -1

// unsigned
static_assert(div<asymmetric>(1u, 2u) == 1);
static_assert(div<asymmetric>(1u, 4u) == 0);
static_assert(div<asymmetric>(3u, 4u) == 1);
static_assert(div<asymmetric>(1u, 3u) == 0);
static_assert(div<asymmetric>(2u, 3u) == 1);

// div_shr
// --------------------------------------------------------------------------------------------------------------------

// fallthrough to div
static_assert(div_shr<asymmetric>(1, 2, 0) == 1);  //  0.5 -> 1
static_assert(div_shr<asymmetric>(-1, 2, 0) == 0); // -0.5 -> 0

// fallthrough to shr
static_assert(div_shr<asymmetric>(6, 3, 1) == 1); // q=2, r=0, shr(2,1) -> 1

// signed, positive quotient, frac at half
// near-tie resolved by positive remainder -> up
// q=1, r=1, frac=1 half=1 neg=0, 1 >= 1 -> carry
// 4/6 = 0.667 -> 1
static_assert(div_shr<asymmetric>(4, 3, 1) == 1);

// signed, negative quotient, frac at half
// near-tie resolved by negative remainder -> down toward +inf
// q=-1, r=-1, frac=uint(-1)&1=1, half=1, neg=1, 1 >= 2 -> no carry
// -4/6 = -0.667 -> -1
static_assert(div_shr<asymmetric>(-4, 3, 1) == -1);

// signed, frac at half, shift=2, positive quotient
// q=2, r=1, frac=2&3=2, half=2, neg=0, 2 >= 2 -> carry
// shifted=0, result=1, 7/12 = 0.583 -> 1
static_assert(div_shr<asymmetric>(7, 3, 2) == 1);

// signed, frac at half, shift=2, negative quotient
// q=-2, r=-1, frac=uint(-2)&3=2, half=2, neg=1, 2 >= 3 -> no carry
// shifted=-1, result=-1, -7/12 = -0.583 -> -1
static_assert(div_shr<asymmetric>(-7, 3, 2) == -1);

// signed, frac below half
// q=-3, r=-1, frac=uint(-3)&3=1, half=2, 1 >= anything -> no carry
// shifted=-1, result=-1, -10/12 = -0.833 -> -1
static_assert(div_shr<asymmetric>(-10, 3, 2) == -1);

// signed, frac above half
// q=3, r=2, frac=3&3=3, half=2, neg=0, 3 >= 2 -> carry
// shifted=0, result=1, 11/12 = 0.917 -> 1
static_assert(div_shr<asymmetric>(11, 3, 2) == 1);

// unsigned
static_assert(div_shr<asymmetric>(7u, 3u, 1) == 1); // q=2, r=1, shr(2,1)=1
static_assert(div_shr<asymmetric>(4u, 3u, 1) == 1); // q=1, r=1, shr(1,1)=1

// uint32_t overflow
// --------------------------------------------------------------------------------------------------------------------

// div: (remainder + 1) > (divisor - remainder)
static_assert(div<asymmetric, uint32_t>(2147483647u, 4294967295u) == 0); // just below half -> 0
static_assert(div<asymmetric, uint32_t>(2147483648u, 4294967295u) == 1); // just above half -> 1
static_assert(div<asymmetric, uint32_t>(3000000000u, 4294967295u) == 1); // well above half -> 1
static_assert(div<asymmetric, uint32_t>(4294967294u, 4294967295u) == 1); // max remainder -> 1
static_assert(div<asymmetric, uint32_t>(2147483647u, 4294967294u) == 1); // near-max even divisor, tie -> 1

// shr: large values
static_assert(shr<asymmetric, uint32_t>(0xFFFFFFFFu, 1) == 0x80000000u); // tie -> up
static_assert(shr<asymmetric, uint32_t>(0x80008000u, 16) == 0x8001u);    // tie -> up
static_assert(shr<asymmetric, uint32_t>(0x80007FFFu, 16) == 0x8000u);    // just below half -> down

// boundary
// --------------------------------------------------------------------------------------------------------------------
static_assert(shr<asymmetric>(0, 2) == 0);
static_assert(div<asymmetric>(0, 7) == 0);
static_assert(div<asymmetric>(-5, 1) == -5);

// ====================================================================================================================
// Symmetric
// ====================================================================================================================

// shr
// --------------------------------------------------------------------------------------------------------------------

// shift = 1
static_assert(shr<symmetric>(4, 1) == 2);   // exact
static_assert(shr<symmetric>(3, 1) == 2);   //  1.5 ->  2 (away from 0)
static_assert(shr<symmetric>(-3, 1) == -2); // -1.5 -> -2 (away from 0)
static_assert(shr<symmetric>(-1, 1) == -1); // -0.5 -> -1 (away from 0)

// shift = 2
static_assert(shr<symmetric>(1, 2) == 0);   //  0.25 -> 0
static_assert(shr<symmetric>(2, 2) == 1);   //  0.5  -> 1 (away from 0)
static_assert(shr<symmetric>(3, 2) == 1);   //  0.75 -> 1
static_assert(shr<symmetric>(-1, 2) == 0);  // -0.25 -> 0
static_assert(shr<symmetric>(-2, 2) == -1); // -0.5  -> -1 (away from 0)
static_assert(shr<symmetric>(-3, 2) == -1); // -0.75 -> -1

// unsigned
static_assert(shr<symmetric>(3u, 1) == 2); // 1.5 tie -> up (away from 0)
static_assert(shr<symmetric>(1u, 2) == 0); // 0.25 below
static_assert(shr<symmetric>(2u, 2) == 1); // 0.5  tie -> up
static_assert(shr<symmetric>(3u, 2) == 1); // 0.75 above

// div
// --------------------------------------------------------------------------------------------------------------------

// signed, even divisor
static_assert(div<symmetric>(1, 2) == 1);   //  0.5 ->  1 (away from 0)
static_assert(div<symmetric>(-1, 2) == -1); // -0.5 -> -1 (away from 0)
static_assert(div<symmetric>(1, 4) == 0);
static_assert(div<symmetric>(3, 4) == 1);
static_assert(div<symmetric>(-2, 4) == -1); // -0.5 -> -1 (away from 0)

// signed, odd divisor
static_assert(div<symmetric>(1, 3) == 0);
static_assert(div<symmetric>(2, 3) == 1);
static_assert(div<symmetric>(-2, 3) == -1);

// unsigned
static_assert(div<symmetric>(1u, 2u) == 1); // 0.5  tie -> up
static_assert(div<symmetric>(1u, 4u) == 0); // 0.25 below
static_assert(div<symmetric>(2u, 4u) == 1); // 0.5  tie -> up
static_assert(div<symmetric>(3u, 4u) == 1); // 0.75 above
static_assert(div<symmetric>(1u, 3u) == 0); // 0.33 below (odd divisor)
static_assert(div<symmetric>(2u, 3u) == 1); // 0.67 above (odd divisor)

// div_shr
// --------------------------------------------------------------------------------------------------------------------

static_assert(div_shr<symmetric>(1, 2, 0) == 1);   //  0.5 ->  1 (div path)
static_assert(div_shr<symmetric>(-1, 2, 0) == -1); // -0.5 -> -1 (div path)

// shift > 0: shr sees quotient, remainder doesn't matter

// q=2, shr(2,1) = 1 (exact, no rounding)
static_assert(div_shr<symmetric>(7, 3, 1) == 1);

// q=-2, shr(-2,1) = -1 (exact)
static_assert(div_shr<symmetric>(-7, 3, 1) == -1);

// q=1, shr(1,1) = 0.5 -> 1 (away from 0)
static_assert(div_shr<symmetric>(4, 3, 1) == 1);

// q=-1, shr(-1,1) = -0.5 -> -1 (away from 0)
static_assert(div_shr<symmetric>(-4, 3, 1) == -1);

// uint32_t overflow
// --------------------------------------------------------------------------------------------------------------------

static_assert(div<symmetric, uint32_t>(2147483647u, 4294967295u) == 0);
static_assert(div<symmetric, uint32_t>(2147483648u, 4294967295u) == 1);
static_assert(div<symmetric, uint32_t>(4294967294u, 4294967295u) == 1);
static_assert(div<symmetric, uint32_t>(2147483647u, 4294967294u) == 1); // tie -> 1

static_assert(shr<symmetric, uint32_t>(0xFFFFFFFFu, 1) == 0x80000000u);
static_assert(shr<symmetric, uint32_t>(0x80008000u, 16) == 0x8001u); // tie -> up (away from 0)
static_assert(shr<symmetric, uint32_t>(0x80007FFFu, 16) == 0x8000u); // below half -> down

// boundary
// --------------------------------------------------------------------------------------------------------------------
static_assert(shr<symmetric>(0, 2) == 0);
static_assert(div<symmetric>(0, 7) == 0);
static_assert(div<symmetric>(-5, 1) == -5);

// ====================================================================================================================
// Round Nearest Even (RNE)
// ====================================================================================================================

// shr
// --------------------------------------------------------------------------------------------------------------------

// shift = 1: one per (parity, sign)
// tiebreaker is shifted quotient oddness
static_assert(shr<round_nearest_even>(1, 1) == 0);   //  0.5, q=0 even -> keep
static_assert(shr<round_nearest_even>(3, 1) == 2);   //  1.5, q=1 odd  -> up
static_assert(shr<round_nearest_even>(5, 1) == 2);   //  2.5, q=2 even -> keep
static_assert(shr<round_nearest_even>(7, 1) == 4);   //  3.5, q=3 odd  -> up
static_assert(shr<round_nearest_even>(-1, 1) == 0);  // -0.5, shifted=-1 odd  -> up to 0
static_assert(shr<round_nearest_even>(-3, 1) == -2); // -1.5, shifted=-2 even -> keep at -2
static_assert(shr<round_nearest_even>(-5, 1) == -2); // -2.5, shifted=-3 odd  -> up to -2
static_assert(shr<round_nearest_even>(-7, 1) == -4); // -3.5, shifted=-4 even -> keep at -4

// shift = 2: non-ties and ties with both parities
static_assert(shr<round_nearest_even>(1, 2) == 0);    //  0.25, below half
static_assert(shr<round_nearest_even>(3, 2) == 1);    //  0.75, above half
static_assert(shr<round_nearest_even>(2, 2) == 0);    //  0.5, shifted=0 even -> keep
static_assert(shr<round_nearest_even>(6, 2) == 2);    //  1.5, shifted=1 odd  -> up
static_assert(shr<round_nearest_even>(-2, 2) == 0);   // -0.5, shifted=-1 odd -> up to 0
static_assert(shr<round_nearest_even>(-6, 2) == -2);  // -1.5, shifted=-2 even -> keep
static_assert(shr<round_nearest_even>(-10, 2) == -2); // -2.5, shifted=-3 odd -> up to -2

// unsigned
static_assert(shr<round_nearest_even>(1u, 1) == 0); // 0.5, q=0 even -> keep
static_assert(shr<round_nearest_even>(3u, 1) == 2); // 1.5, q=1 odd  -> up
static_assert(shr<round_nearest_even>(5u, 1) == 2); // 2.5, q=2 even -> keep
static_assert(shr<round_nearest_even>(7u, 1) == 4); // 3.5, q=3 odd  -> up
static_assert(shr<round_nearest_even>(2u, 2) == 0); // 0.5 tie, even -> keep
static_assert(shr<round_nearest_even>(6u, 2) == 2); // 1.5 tie, odd  -> up

// div
// --------------------------------------------------------------------------------------------------------------------

// signed, even divisor
static_assert(div<round_nearest_even>(1, 2) == 0);   //  0.5, q=0 even -> keep
static_assert(div<round_nearest_even>(3, 2) == 2);   //  1.5, q=1 odd  -> up
static_assert(div<round_nearest_even>(5, 2) == 2);   //  2.5, q=2 even -> keep
static_assert(div<round_nearest_even>(7, 2) == 4);   //  3.5, q=3 odd  -> up
static_assert(div<round_nearest_even>(-1, 2) == 0);  // -0.5, q=0 even -> keep
static_assert(div<round_nearest_even>(-3, 2) == -2); // -1.5, q=-1 odd -> round
static_assert(div<round_nearest_even>(-5, 2) == -2); // -2.5, q=-2 even -> keep
static_assert(div<round_nearest_even>(-7, 2) == -4); // -3.5, q=-3 odd -> round

// non-ties
static_assert(div<round_nearest_even>(1, 4) == 0); //  0.25, below
static_assert(div<round_nearest_even>(3, 4) == 1); //  0.75, above
static_assert(div<round_nearest_even>(2, 4) == 0); //  0.5 tie, q=0 even -> keep
static_assert(div<round_nearest_even>(6, 4) == 2); //  1.5 tie, q=1 odd  -> up

// signed, odd divisor
static_assert(div<round_nearest_even>(1, 3) == 0);
static_assert(div<round_nearest_even>(2, 3) == 1);
static_assert(div<round_nearest_even>(-1, 3) == 0);
static_assert(div<round_nearest_even>(-2, 3) == -1);

// unsigned
static_assert(div<round_nearest_even>(1u, 2u) == 0); // 0.5, q=0 even -> keep
static_assert(div<round_nearest_even>(3u, 2u) == 2); // 1.5, q=1 odd  -> up
static_assert(div<round_nearest_even>(5u, 2u) == 2); // 2.5, q=2 even -> keep
static_assert(div<round_nearest_even>(1u, 3u) == 0);
static_assert(div<round_nearest_even>(2u, 3u) == 1);

// div_shr
// --------------------------------------------------------------------------------------------------------------------

// shift = 0: div path
static_assert(div_shr<round_nearest_even>(1, 2, 0) == 0); //  0.5, q=0 even -> keep
static_assert(div_shr<round_nearest_even>(3, 2, 0) == 2); //  1.5, q=1 odd  -> up

// rem = 0: shr path
static_assert(div_shr<round_nearest_even>(6, 3, 1) == 1); // q=2, exact, shr(2,1) = 1

// signed, positive quotient, frac at half: near-tie resolved up
// q=1, r=1, frac=1, half=1, tiebreaker=(1>=0)=1, bias=0+1=1
// (1+1)>>1 = 1, shifted=0, result=1, 4/6 = 0.667 -> 1
static_assert(div_shr<round_nearest_even>(4, 3, 1) == 1);

// signed, negative quotient, frac at half: near-tie resolved down
// q=-1 r=-1, frac=uint(-1)&1=1, half=1, tiebreaker=(-1>=0)=0, bias=0
// (1+0)>>1 = 0, shifted=-1, result=-1, -4/6 = -0.667 -> -1
static_assert(div_shr<round_nearest_even>(-4, 3, 1) == -1);

// shift = 2, positive quotient at half
// q=2 r=1, frac=2&3=2, half=2, tiebreaker=1, bias=1+1=2
// (2+2)>>2 = 1, shifted=0, result=1, 7/12 = 0.583 -> 1
static_assert(div_shr<round_nearest_even>(7, 3, 2) == 1);

// shift = 2, negative quotient at half
// q=-2 r=-1, frac=uint(-2)&3=2, half=2, tiebreaker=0, bias=1
// (2+1)>>2 = 0, shifted=-1, result=-1, -7/12 = -0.583 -> -1
static_assert(div_shr<round_nearest_even>(-7, 3, 2) == -1);

// frac above half: carries regardless
// q=3 r=1, frac=1, half=1, tiebreaker=1, bias=1
// (1+1)>>1 = 1, shifted=1, result=2, 10/6 = 1.667 -> 2
static_assert(div_shr<round_nearest_even>(10, 3, 1) == 2);

// frac above half: carries regardless
// q=-3 r=-1, frac=1, tiebreaker=0, bias=0
// (1+0)>>1 = 0, shifted=-2, result=-2, -10/6 = -1.667 -> -2
static_assert(div_shr<round_nearest_even>(-10, 3, 1) == -2);

// frac below half: never carries

// q=1 r=2, frac=1&3=1, half=2, 1+2=3, 3>>2=0, shifted=0, result=0
// 5/12 = 0.417 -> 0
static_assert(div_shr<round_nearest_even>(5, 3, 2) == 0);

// unsigned
static_assert(div_shr<round_nearest_even>(4u, 3u, 1) == 1);  // 4/6 = 0.667 -> 1
static_assert(div_shr<round_nearest_even>(16u, 3u, 1) == 3); // 16/6 = 2.667 -> 3
static_assert(div_shr<round_nearest_even>(7u, 3u, 2) == 1);  // 7/12 = 0.583 -> 1
static_assert(div_shr<round_nearest_even>(19u, 3u, 2) == 2); // 19/12 = 1.583 -> 2

// uint32_t overflow
// --------------------------------------------------------------------------------------------------------------------

// div: (remainder + is_odd) > (divisor - remainder)

static_assert(div<round_nearest_even, uint32_t>(2147483647u, 4294967295u) == 0); // below half, q=0 even
static_assert(div<round_nearest_even, uint32_t>(2147483648u, 4294967295u) == 1); // above half, q=0 even
static_assert(div<round_nearest_even, uint32_t>(4294967294u, 4294967295u) == 1); // near 1.0
static_assert(div<round_nearest_even, uint32_t>(2147483647u, 4294967294u) == 0); // 0.5 tie, q=0 even -> keep

// odd quotient with large odd divisor
//   3221225470 / 2147483647 = 1 r 1073741823, is_odd=1
//   (1073741823 + 1) > (2147483647 - 1073741823) -> 1073741824 > 1073741824 -> false -> 1
//   1.49999999953... -> 1
static_assert(div<round_nearest_even, uint32_t>(3221225470u, 2147483647u) == 1);

//   3221225471 / 2147483647 = 1 r 1073741824, is_odd=1
//   (1073741824 + 1) > (2147483647 - 1073741824) -> 1073741825 > 1073741823 -> true -> 2
//   1.50000000047... -> 2
static_assert(div<round_nearest_even, uint32_t>(3221225471u, 2147483647u) == 2);

// shr: (frac + bias) at large values
static_assert(shr<round_nearest_even, uint32_t>(0xFFFFFFFFu, 1) == 0x80000000u); // tie, shifted odd -> up
static_assert(shr<round_nearest_even, uint32_t>(0xFFFFFFFDu, 1) == 0x7FFFFFFEu); // tie, shifted even -> keep
static_assert(shr<round_nearest_even, uint32_t>(0xFFFFFFFEu, 1) == 0x7FFFFFFFu); // exact (no rounding)
static_assert(shr<round_nearest_even, uint32_t>(0x80008000u, 16) == 0x8000u);    // tie, shifted even -> keep
static_assert(shr<round_nearest_even, uint32_t>(0x80018000u, 16) == 0x8002u);    // tie, shifted odd  -> up

// div_shr: large values

// 4294967291 / 3 = 1431655763 r 2, 4294967291/6 = 715827881.83 -> 715827882
// q odd, frac=1, tiebreaker=1, carry=1, shifted=715827881, result=715827882
static_assert(div_shr<round_nearest_even, uint32_t>(4294967291u, 3u, 1) == 715827882u);

// 4294967289 / 3 = 1431655763 r 0, falls to shr, q odd.
// 4294967289/6 = 715827881.5 -> 715827882 (odd -> up)
static_assert(div_shr<round_nearest_even, uint32_t>(4294967289u, 3u, 1) == 715827882u);

// 4294967293 / 3 = 1431655764 r 1, q even, frac=0
// 4294967293/6 = 715827882.17 -> 715827882, Below half, no carry
static_assert(div_shr<round_nearest_even, uint32_t>(4294967293u, 3u, 1) == 715827882u);

// boundary
// --------------------------------------------------------------------------------------------------------------------
static_assert(shr<round_nearest_even>(0, 2) == 0);
static_assert(div<round_nearest_even>(0, 7) == 0);
static_assert(div<round_nearest_even>(5, 1) == 5);
static_assert(shr<round_nearest_even>(8, 4) == 0);     // 0.5, q=0 even -> keep
static_assert(shr<round_nearest_even>(24, 4) == 2);    // 1.5, q=1 odd  -> up
static_assert(div<round_nearest_even>(50, 100) == 0);  // 0.5, q=0 even -> keep
static_assert(div<round_nearest_even>(150, 100) == 2); // 1.5, q=1 odd  -> up

} // namespace
} // namespace crv::rounding_modes
