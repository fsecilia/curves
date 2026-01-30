// SPDX-License-Identifier: MIT
/*!
    \file
    \brief io for math types

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/integer.hpp>
#include <ostream>

namespace crv {

auto operator<<(std::ostream& out, uint128_t src) -> std::ostream&;
auto operator<<(std::ostream& out, int128_t src) -> std::ostream&;
auto operator<<(std::ostream& out, div_u128_u64_t const& src) -> std::ostream&;

template <integral value_type, int_t frac_bits>
auto operator<<(std::ostream& out, fixed_t<value_type, frac_bits> const& src) -> std::ostream&
{
    return out << src.value;
}

} // namespace crv
