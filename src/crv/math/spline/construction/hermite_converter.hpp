// SPDX-License-Identifier: MIT

/// \file
/// \brief cubic basis transform
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#if 0

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

        auto const a = -2.0 * dp + m0 + m1;
        auto const b = 3.0 * dp - 2.0 * m0 - m1;
        auto const c = m0;
        auto const d = p0;

        return {{to_fixed<coeff_t>(a), to_fixed<coeff_t>(b), to_fixed<coeff_t>(c), to_fixed<coeff_t>(d)}};
    }
};

} // namespace crv::spline

#endif
