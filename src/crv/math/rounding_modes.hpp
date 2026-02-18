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

} // namespace crv::rounding_modes
