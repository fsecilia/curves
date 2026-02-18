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

} // namespace
} // namespace crv::rounding_modes
