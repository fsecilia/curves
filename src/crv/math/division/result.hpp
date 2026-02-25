// SPDX-License-Identifier: MIT
/*!
    \file
    \brief division results

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <compare>

namespace crv::division {

//! result of division in full (quotient, remainder) form
template <typename quotient_t, typename remainder_t = quotient_t> struct result_t
{
    quotient_t  quotient;
    remainder_t remainder;

    auto operator<=>(result_t const&) const noexcept -> auto = default;
    auto operator==(result_t const&) const noexcept -> bool  = default;
};

} // namespace crv
