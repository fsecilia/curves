// SPDX-License-Identifier: MIT

/// \file
/// \brief cubic basis transform
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/spline/polynomial.hpp>

namespace crv::spline {

/// converts float hermite to fixed polynomial
template <is_fixed coeff_t> struct hermite_converter_t
{
    template <typename jet_t>
    constexpr auto operator()(jet_t left_y, jet_t right_y) const noexcept -> cubic_polynomial_t<coeff_t>
    {
        auto const p0 = primal(left_y);
        auto const p1 = primal(right_y);
        auto const m0 = tangent(left_y);
        auto const m1 = tangent(right_y);
        auto const dp = p1 - p0;
        return {{
            to_fixed<coeff_t>(-2.0 * dp + m0 + m1),
            to_fixed<coeff_t>(3.0 * dp - 2.0 * m0 - m1),
            to_fixed<coeff_t>(m0),
            to_fixed<coeff_t>(p0),
        }};
    }
};

} // namespace crv::spline
