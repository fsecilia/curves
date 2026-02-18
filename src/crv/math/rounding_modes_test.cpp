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

} // namespace
} // namespace crv::rounding_modes
