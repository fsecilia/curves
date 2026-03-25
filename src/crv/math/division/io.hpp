// SPDX-License-Identifier: MIT

/// \file
/// \brief io for division types
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/division/result.hpp>
#include <crv/math/io.hpp>
#include <ostream>

namespace crv::division {

using crv::operator<<;

template <typename narrow_t> auto operator<<(std::ostream& out, qr_pair_t<narrow_t> const& src) -> std::ostream&
{
    return out << "{.quotient = " << src.quotient << ", .remainder = " << src.remainder << "}";
}

} // namespace crv::division
