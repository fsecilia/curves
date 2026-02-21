// SPDX-License-Identifier: MIT
/*!
    \file
    \brief

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <compare>

namespace crv {

//! result of u128/u64
struct div_u128_u64_t
{
    uint64_t quotient;
    uint64_t remainder;

    auto operator<=>(div_u128_u64_t const&) const noexcept -> auto = default;
    auto operator==(div_u128_u64_t const&) const noexcept -> bool  = default;
};

} // namespace crv
