// SPDX-License-Identifier: MIT
/*!
    \file
    \brief io for math types

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <crv/math/division/result.hpp>
#include <crv/math/integer.hpp>
#include <ostream>

namespace crv {

auto operator<<(std::ostream& out, uint128_t src) -> std::ostream&;
auto operator<<(std::ostream& out, int128_t src) -> std::ostream&;

namespace division {

using crv::operator<<;

template <typename quotient_t, typename remainder_t>
auto operator<<(std::ostream& out, result_t<quotient_t, remainder_t> const& src) -> std::ostream&
{
    return out << "{.quotient = " << src.quotient << ", .remainder = " << src.remainder << "}";
}

} // namespace division
} // namespace crv
