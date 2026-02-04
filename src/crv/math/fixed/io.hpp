// SPDX-License-Identifier: MIT
/*!
    \file
    \brief io for fixed_t

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float.hpp>
#include <ostream>

namespace crv {

template <integral value_type, int_t frac_bits>
auto operator<<(std::ostream& out, fixed_t<value_type, frac_bits> const& src) -> std::ostream&
{
    return out << from_fixed<float_t>(src);
}

} // namespace crv
