// SPDX-License-Identifier: MIT
/*!
    \file
    \brief provides an accumulator using Kahan summation to compensate for precision loss during addition

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>

namespace crv {

/*!
    accumulates a sum using Kahan summation

    Kahan summation tracks the error from each addition, then reintroduces it in the next, increasing the accuracy of
    the sum overall.

    This is a minimal implementation meant to be a drop-in replacement for simple sums consisting soley as a series of
    operator +=, then reading the final value.
*/
template <typename real_t> struct compensated_accumulator_t
{
    real_t sum          = real_t{0};
    real_t compensation = real_t{0};

    auto operator+=(real_t value) -> void
    {
        auto const y = value - compensation;
        auto const t = sum + y;
        compensation = (t - sum) - y;
        sum          = t;
    }

    operator real_t() const { return sum + compensation; }

    constexpr auto operator<=>(compensated_accumulator_t const&) const noexcept -> auto = default;
    constexpr auto operator==(compensated_accumulator_t const&) const noexcept -> bool  = default;
};

} // namespace crv
