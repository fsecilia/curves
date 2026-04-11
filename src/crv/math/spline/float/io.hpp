// SPDX-License-Identifier: MIT

/// \file
/// \brief ostream inserters for floating point spline modules
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/spline/float/segment.hpp>
#include <ostream>

namespace crv::spline::floating_point {

template <typename real_t> auto operator<<(std::ostream& out, segment_t<real_t> const& src) -> std::ostream&
{
    using src_t = segment_t<real_t>;

    out << "{" << src.coeffs[0];
    for (auto coeff = 1; coeff < src_t::coeff_count; ++coeff) out << ", " << src.coeffs[coeff];
    out << "}";
    return out;
}

} // namespace crv::spline::floating_point
