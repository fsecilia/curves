// SPDX-License-Identifier: MIT

/// \file
/// \brief ostream inserters for fixed-point spline modules
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/spline/fixed/cubic_monomial.hpp>
#include <ostream>

namespace crv::spline::fixed_point {

template <integral value_t> auto operator<<(std::ostream& out, cubic_monomial_t<value_t> const& src) -> std::ostream&
{
    using src_t = cubic_monomial_t<value_t>;

    out << "{.coeffs = {" << src.coeffs[0];
    for (auto coeff = 1; coeff < src_t::coeff_count; ++coeff) out << ", " << src.coeffs[coeff];
    out << "}, .shifts = {" << src.shifts[0];
    for (auto coeff = 1; coeff < src_t::coeff_count - 1; ++coeff) out << ", " << src.shifts[coeff];
    out << "}, .final_frac_bits = " << src.final_frac_bits << "}";

    return out;
}

} // namespace crv::spline::fixed_point
