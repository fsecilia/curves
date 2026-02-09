// SPDX-License-Identifier: MIT
/*!
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include "io.hpp"
#include <array>

namespace crv {

auto operator<<(std::ostream& out, uint128_t src) -> std::ostream&
{
    if (src == 0) return out << "0";

    std::array<char, 39> buffer;

    auto cur = std::end(buffer);
    while (src > 0)
    {
        *--cur = '0' + static_cast<char>(src % 10);
        src /= 10;
    }

    out.write(cur, std::end(buffer) - cur);

    return out;
}

auto operator<<(std::ostream& out, int128_t src) -> std::ostream&
{
    if (src < 0) { return out << "-" << -static_cast<uint128_t>(src); }
    else return out << static_cast<uint128_t>(src);
}

auto operator<<(std::ostream& out, div_u128_u64_t const& src) -> std::ostream&
{
    return out << "{.quotient = " << src.quotient << ", .remainder = " << src.remainder << "}";
}

} // namespace crv
