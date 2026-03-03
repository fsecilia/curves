// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/math/limits.hpp>
#include <crv/math/rounding_modes.hpp>
#include <crv/test/test.hpp>

namespace crv::rounding_modes {
namespace {

template <typename value_t, typename sut_t> constexpr auto shr(value_t unshifted, int_t shift, sut_t sut) -> value_t
{
    return sut.shr(static_cast<value_t>(unshifted >> shift), unshifted, shift);
}

template <typename value_t, typename sut_t> constexpr auto div(value_t dividend, value_t divisor, sut_t sut) -> value_t
{
    return sut.div(static_cast<value_t>(dividend / divisor), divisor, static_cast<value_t>(dividend % divisor));
}

template <typename value_t, typename sut_t>
constexpr auto div_shr(value_t dividend, value_t divisor, int_t shift, sut_t sut) -> value_t
{
    auto const quotient         = static_cast<value_t>(dividend / divisor);
    auto const remainder        = static_cast<value_t>(dividend % divisor);
    auto const shifted_quotient = static_cast<value_t>(quotient >> shift);

    return sut.div_shr(shifted_quotient, quotient, divisor, remainder, shift);
}

// ====================================================================================================================
// truncate
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// shr
// --------------------------------------------------------------------------------------------------------------------

// zero
static_assert(shr(0, 2, truncate) == 0);

// exact
static_assert(shr(4, 1, truncate) == 2);
static_assert(shr(-4, 1, truncate) == -2);

// inexact
static_assert(shr(3, 1, truncate) == 1);
static_assert(shr(-3, 1, truncate) == -2);

// unsigned
static_assert(shr(3u, 1, truncate) == 1);

// --------------------------------------------------------------------------------------------------------------------
// div
// --------------------------------------------------------------------------------------------------------------------

// zero
static_assert(div(0, 7, truncate) == 0);

// exact
static_assert(div(5, 1, truncate) == 5);
static_assert(div(-5, 1, truncate) == -5);

// inexact
static_assert(div(3, 2, truncate) == 1);
static_assert(div(-3, 2, truncate) == -1);

// unsigned
static_assert(div(3u, 2u, truncate) == 1);

// --------------------------------------------------------------------------------------------------------------------
// div_shr
// --------------------------------------------------------------------------------------------------------------------

// shift == 0
static_assert(div_shr(3, 2, 0, truncate) == 1);

// remainder == 0
static_assert(div_shr(6, 3, 1, truncate) == 1);

// remainder != 0
static_assert(div_shr(7, 3, 1, truncate) == 1);
static_assert(div_shr(-7, 3, 1, truncate) == -1);

// ====================================================================================================================
// nearest_up
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// shr
// --------------------------------------------------------------------------------------------------------------------

// zero
static_assert(shr(0, 2, nearest_up) == 0);

// exact
static_assert(shr(4, 1, nearest_up) == 2);
static_assert(shr(-4, 1, nearest_up) == -2);

// inexact
static_assert(shr(1, 2, nearest_up) == 0);
static_assert(shr(3, 2, nearest_up) == 1);
static_assert(shr(-1, 2, nearest_up) == 0);
static_assert(shr(-3, 2, nearest_up) == -1);

// ties
static_assert(shr(3, 1, nearest_up) == 2);
static_assert(shr(-3, 1, nearest_up) == -1);
static_assert(shr(-1, 1, nearest_up) == 0);
static_assert(shr(2, 2, nearest_up) == 1);
static_assert(shr(-2, 2, nearest_up) == 0);

// unsigned
static_assert(shr(1u, 2, nearest_up) == 0);
static_assert(shr(3u, 1, nearest_up) == 2);
static_assert(shr(2u, 2, nearest_up) == 1);
static_assert(shr(3u, 2, nearest_up) == 1);

// boundaries
static_assert(shr<uint32_t>(0x80007FFFu, 16, nearest_up) == 0x8000u);    // below half
static_assert(shr<uint32_t>(0xFFFFFFFFu, 1, nearest_up) == 0x80000000u); // tie -> up
static_assert(shr<uint32_t>(0x80008000u, 16, nearest_up) == 0x8001u);    // tie -> up

// --------------------------------------------------------------------------------------------------------------------
// div
// --------------------------------------------------------------------------------------------------------------------

// zero
static_assert(div(0, 7, nearest_up) == 0);

// exact
static_assert(div(5, 1, nearest_up) == 5);
static_assert(div(-5, 1, nearest_up) == -5);

// inexact
static_assert(div(1, 4, nearest_up) == 0);
static_assert(div(3, 4, nearest_up) == 1);
static_assert(div(-1, 4, nearest_up) == 0);
static_assert(div(-3, 4, nearest_up) == -1);

// inexact
static_assert(div(1, 3, nearest_up) == 0);
static_assert(div(2, 3, nearest_up) == 1);
static_assert(div(-2, 3, nearest_up) == -1);

// ties
static_assert(div(1, 2, nearest_up) == 1);
static_assert(div(-1, 2, nearest_up) == 0);

// unsigned
static_assert(div(1u, 4u, nearest_up) == 0);
static_assert(div(3u, 4u, nearest_up) == 1);
static_assert(div(1u, 3u, nearest_up) == 0);
static_assert(div(2u, 3u, nearest_up) == 1);
static_assert(div(1u, 2u, nearest_up) == 1);

// boundaries
static_assert(div<uint32_t>(2147483647u, 4294967295u, nearest_up) == 0); // below half
static_assert(div<uint32_t>(2147483648u, 4294967295u, nearest_up) == 1); // above half
static_assert(div<uint32_t>(4294967294u, 4294967295u, nearest_up) == 1); // near 1.0
static_assert(div<uint32_t>(2147483647u, 4294967294u, nearest_up) == 1); // tie -> up

// --------------------------------------------------------------------------------------------------------------------
// div_shr
// --------------------------------------------------------------------------------------------------------------------

// shift == 0
static_assert(div_shr(1, 2, 0, nearest_up) == 1);  // tie -> up
static_assert(div_shr(-1, 2, 0, nearest_up) == 0); // tie -> up

// remainder == 0
static_assert(div_shr(6, 2, 1, nearest_up) == 2);   // tie -> up
static_assert(div_shr(-6, 2, 1, nearest_up) == -1); // tie -> up

// remainder != 0
static_assert(div_shr(5, 2, 1, nearest_up) == 1);   // exact
static_assert(div_shr(3, 2, 1, nearest_up) == 1);   // tie -> away from zero
static_assert(div_shr(-3, 2, 1, nearest_up) == -1); // tie -> away from zero

// unsigned
static_assert(div_shr(5u, 2u, 1, nearest_up) == 1u); // exact
static_assert(div_shr(7u, 2u, 1, nearest_up) == 2u); // tie -> away

// boundaries
static_assert(div_shr<uint32_t>(0xFFFFFFFFu, 2u, 1, nearest_up) == 0x40000000u); // tie -> away -> up

// ====================================================================================================================
// nearest_away
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// shr
// --------------------------------------------------------------------------------------------------------------------

// zero
static_assert(shr(0, 2, nearest_away) == 0);

// exact
static_assert(shr(4, 1, nearest_away) == 2);

// inexact
static_assert(shr(1, 2, nearest_away) == 0);
static_assert(shr(3, 2, nearest_away) == 1);
static_assert(shr(-1, 2, nearest_away) == 0);
static_assert(shr(-3, 2, nearest_away) == -1);

// ties
static_assert(shr(3, 1, nearest_away) == 2);
static_assert(shr(-3, 1, nearest_away) == -2);
static_assert(shr(-1, 1, nearest_away) == -1);
static_assert(shr(2, 2, nearest_away) == 1);
static_assert(shr(-2, 2, nearest_away) == -1);

// unsigned
static_assert(shr(3u, 1, nearest_away) == 2);
static_assert(shr(2u, 2, nearest_away) == 1);

// boundaries
static_assert(shr<uint32_t>(0x80007FFFu, 16, nearest_away) == 0x8000u);    // below half
static_assert(shr<uint32_t>(0xFFFFFFFFu, 1, nearest_away) == 0x80000000u); // tie -> away
static_assert(shr<uint32_t>(0x80008000u, 16, nearest_away) == 0x8001u);    // tie -> away

// --------------------------------------------------------------------------------------------------------------------
// div
// --------------------------------------------------------------------------------------------------------------------

// zero
static_assert(div(0, 7, nearest_away) == 0);

// exact
static_assert(div(-5, 1, nearest_away) == -5);

// inexact
static_assert(div(1, 4, nearest_away) == 0);
static_assert(div(3, 4, nearest_away) == 1);

// inexact
static_assert(div(1, 3, nearest_away) == 0);
static_assert(div(2, 3, nearest_away) == 1);
static_assert(div(-2, 3, nearest_away) == -1);

// ties
static_assert(div(1, 2, nearest_away) == 1);
static_assert(div(-1, 2, nearest_away) == -1);
static_assert(div(-2, 4, nearest_away) == -1);

// unsigned
static_assert(div(1u, 4u, nearest_away) == 0);
static_assert(div(3u, 4u, nearest_away) == 1);
static_assert(div(1u, 2u, nearest_away) == 1);

// boundaries
static_assert(div<uint32_t>(2147483647u, 4294967295u, nearest_away) == 0); // below half
static_assert(div<uint32_t>(2147483648u, 4294967295u, nearest_away) == 1); // above half
static_assert(div<uint32_t>(2147483647u, 4294967294u, nearest_away) == 1); // tie -> away -> up

// --------------------------------------------------------------------------------------------------------------------
// div_shr
// --------------------------------------------------------------------------------------------------------------------

// shift == 0
static_assert(div_shr(1, 2, 0, nearest_away) == 1);   // tie -> away
static_assert(div_shr(-1, 2, 0, nearest_away) == -1); // tie -> away

// remainder == 0
static_assert(div_shr(6, 2, 1, nearest_away) == 2);   // tie -> away
static_assert(div_shr(-6, 2, 1, nearest_away) == -2); // tie -> away

// remainder != 0
static_assert(div_shr(3, 2, 1, nearest_away) == 1);   // tie -> away
static_assert(div_shr(-3, 2, 1, nearest_away) == -1); // tie -> away

// unsigned
static_assert(div_shr(5u, 2u, 1, nearest_away) == 1u); // exact
static_assert(div_shr(7u, 2u, 1, nearest_away) == 2u); // tie -> away

// boundaries
static_assert(div_shr<uint32_t>(0xFFFFFFFFu, 2u, 1, nearest_away) == 0x40000000u); // tie -> away -> up

// ====================================================================================================================
// nearest_even
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// shr
// --------------------------------------------------------------------------------------------------------------------

// zero
static_assert(shr(0, 2, nearest_even) == 0);

// exact
static_assert(shr(-7, 1, nearest_even) == -4); // shifted=-4 even -> keep

// inexact
static_assert(shr(1, 2, nearest_even) == 0);
static_assert(shr(3, 2, nearest_even) == 1);

// ties: round based on shifted parity
static_assert(shr(1, 1, nearest_even) == 0);    // shifted=0 even -> keep
static_assert(shr(3, 1, nearest_even) == 2);    // shifted=1 odd  -> round
static_assert(shr(5, 1, nearest_even) == 2);    // shifted=2 even -> keep
static_assert(shr(2, 2, nearest_even) == 0);    // shifted=0 even -> keep
static_assert(shr(6, 2, nearest_even) == 2);    // shifted=1 odd  -> round
static_assert(shr(-1, 1, nearest_even) == 0);   // shifted=-1 odd -> round
static_assert(shr(-3, 1, nearest_even) == -2);  // shifted=-2 even -> keep
static_assert(shr(-2, 2, nearest_even) == 0);   // shifted=-1 odd  -> round
static_assert(shr(-6, 2, nearest_even) == -2);  // shifted=-2 even -> keep
static_assert(shr(-10, 2, nearest_even) == -2); // shifted=-3 odd  -> round

// unsigned
static_assert(shr(1u, 1, nearest_even) == 0); // shifted=0 even -> keep
static_assert(shr(3u, 1, nearest_even) == 2); // shifted=1 odd  -> round
static_assert(shr(5u, 1, nearest_even) == 2); // shifted=2 even -> keep
static_assert(shr(7u, 1, nearest_even) == 4); // shifted=3 odd  -> round
static_assert(shr(2u, 2, nearest_even) == 0); // shifted=0 even -> keep
static_assert(shr(6u, 2, nearest_even) == 2); // shifted=1 odd  -> round

// boundaries
static_assert(shr<uint32_t>(0xFFFFFFFEu, 1, nearest_even) == 0x7FFFFFFFu); // exact
static_assert(shr<uint32_t>(0xFFFFFFFDu, 1, nearest_even) == 0x7FFFFFFEu); // tie, shifted even -> keep
static_assert(shr<uint32_t>(0xFFFFFFFFu, 1, nearest_even) == 0x80000000u); // tie, shifted odd  -> round
static_assert(shr<uint32_t>(0x80008000u, 16, nearest_even) == 0x8000u);    // tie, shifted even -> keep
static_assert(shr<uint32_t>(0x80018000u, 16, nearest_even) == 0x8002u);    // tie, shifted odd  -> round

// --------------------------------------------------------------------------------------------------------------------
// div
// --------------------------------------------------------------------------------------------------------------------

// zero
static_assert(div(0, 7, nearest_even) == 0);

// exact
static_assert(div(5, 1, nearest_even) == 5);

// inexact
static_assert(div(1, 4, nearest_even) == 0);
static_assert(div(3, 4, nearest_even) == 1);

// inexact
static_assert(div(1, 3, nearest_even) == 0);
static_assert(div(2, 3, nearest_even) == 1);
static_assert(div(-1, 3, nearest_even) == 0);
static_assert(div(-2, 3, nearest_even) == -1);

// ties
static_assert(div(1, 2, nearest_even) == 0);   // q = 0 even -> keep
static_assert(div(3, 2, nearest_even) == 2);   // q = 1 odd  -> round
static_assert(div(5, 2, nearest_even) == 2);   // q = 2 even -> keep
static_assert(div(7, 2, nearest_even) == 4);   // q = 3 odd  -> round
static_assert(div(2, 4, nearest_even) == 0);   // q = 0 even -> keep
static_assert(div(6, 4, nearest_even) == 2);   // q = 1 odd  -> round
static_assert(div(-1, 2, nearest_even) == 0);  // q = 0 even -> keep
static_assert(div(-3, 2, nearest_even) == -2); // q = -1 odd -> round
static_assert(div(-5, 2, nearest_even) == -2); // q = -2 even -> keep
static_assert(div(-7, 2, nearest_even) == -4); // q = -3 odd -> round

// unsigned
static_assert(div(1u, 3u, nearest_even) == 0); // below half
static_assert(div(2u, 3u, nearest_even) == 1); // above half
static_assert(div(1u, 2u, nearest_even) == 0); // q = 0 even -> keep
static_assert(div(3u, 2u, nearest_even) == 2); // q = 1 odd  -> round
static_assert(div(5u, 2u, nearest_even) == 2); // q = 2 even -> keep

// boundaries
static_assert(div<uint32_t>(2147483647u, 4294967295u, nearest_even) == 0); // below half, q = 0 even
static_assert(div<uint32_t>(2147483648u, 4294967295u, nearest_even) == 1); // above half
static_assert(div<uint32_t>(2147483647u, 4294967294u, nearest_even) == 0); // tie, q = 0 even -> keep
static_assert(div<uint32_t>(3221225470u, 2147483647u, nearest_even) == 1); // below tie, odd q -> keep
static_assert(div<uint32_t>(3221225471u, 2147483647u, nearest_even) == 2); // above tie, odd q -> round

// --------------------------------------------------------------------------------------------------------------------
// div_shr
// --------------------------------------------------------------------------------------------------------------------

// shift == 0
static_assert(div_shr(1, 2, 0, nearest_even) == 0); // tie, q = 0 even -> keep
static_assert(div_shr(3, 2, 0, nearest_even) == 2); // tie, q = 1 odd -> round

// remainder == 0
static_assert(div_shr(10, 2, 1, nearest_even) == 2);  // tie, shifted = 2 even -> keep
static_assert(div_shr(6, 2, 1, nearest_even) == 2);   // tie, shifted = 1 odd -> round
static_assert(div_shr(-6, 2, 1, nearest_even) == -2); // tie, shifted = -2 even -> keep

// remainder != 0
static_assert(div_shr(3, 2, 1, nearest_even) == 1);   // tie -> away
static_assert(div_shr(-3, 2, 1, nearest_even) == -1); // tie -> away

// unsigned
static_assert(div_shr(5u, 2u, 1, nearest_even) == 1u);  // exact
static_assert(div_shr(10u, 2u, 1, nearest_even) == 2u); // tie, rem == 0, shifted even -> keep
static_assert(div_shr(6u, 2u, 1, nearest_even) == 2u);  // tie, rem == 0, shifted odd -> round

// boundaries
static_assert(div_shr<uint32_t>(0xFFFFFFFEu, 2u, 1, nearest_even) == 0x40000000u); // tie, rem == 0, odd  -> round
static_assert(div_shr<uint32_t>(0xFFFFFFFFu, 2u, 1, nearest_even) == 0x40000000u); // tie, rem != 0, away -> round

// ====================================================================================================================
// canned_t
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// div_shr
// --------------------------------------------------------------------------------------------------------------------

struct canned_t : primitives::div_shr_t
{
    static constexpr auto                      expected_shr = 73;
    template <integral value_t> constexpr auto shr(value_t, value_t, int_t) const noexcept -> value_t
    {
        return expected_shr;
    }

    static constexpr auto                      expected_div = 79;
    template <integral value_t> constexpr auto div(value_t, value_t, value_t) const noexcept -> value_t
    {
        return expected_div;
    }
};
constexpr auto canned = canned_t{};

// shift == 0
static_assert(div_shr(1, 2, 0, canned) == canned_t::expected_div);
static_assert(div_shr(-1, 2, 0, canned) == canned_t::expected_div);
static_assert(div_shr(1u, 2u, 0, canned) == canned_t::expected_div);

// remainder == 0
static_assert(div_shr(4, 2, 1, canned) == canned_t::expected_shr);
static_assert(div_shr(-4, 2, 1, canned) == canned_t::expected_shr);
static_assert(div_shr(4u, 2u, 1, canned) == canned_t::expected_shr);

// remainder != 0
static_assert(div_shr(4, 3, 1, canned) == 1);
static_assert(div_shr(-4, 3, 1, canned) == -1);
static_assert(div_shr(4u, 3u, 1, canned) == 1u);

} // namespace
} // namespace crv::rounding_modes
