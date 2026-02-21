// SPDX-License-Identifier: MIT
/*!
    \file
    \brief division results

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <compare>

namespace crv {

//! result of division in {quotient, remainder} form using the type of the original divisor and dividend
template <typename value_t> struct div_result_t
{
    value_t quotient;
    value_t remainder;

    auto operator<=>(div_result_t const&) const noexcept -> auto = default;
    auto operator==(div_result_t const&) const noexcept -> bool  = default;
};

//! result of u128/u64
using div_u128_u64_t = div_result_t<uint64_t>;

} // namespace crv
