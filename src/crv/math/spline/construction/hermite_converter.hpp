// SPDX-License-Identifier: MIT

/// \file
/// \brief cubic basis transform
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/spline/construction/cubic.hpp>

namespace crv::spline {

/// converts hermite basis to monomial basis
template <std::floating_point scalar_t> struct hermite_converter_t
{
    template <typename jet_t> constexpr auto operator()(jet_t left_y, jet_t right_y) const noexcept -> cubic_t<scalar_t>
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

        return {a, b, c, d};
    }
};

} // namespace crv::spline
