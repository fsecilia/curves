// SPDX-License-Identifier: MIT

/// \file
/// \brief derivative and chain rule for a segment
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/jet/jet.hpp>
#include <cmath>
#include <ranges>

namespace crv::spline {

/// evaluates segment derivative and converts between dy/dx and dy/dt using the jacobian, dx/dt
///
/// Calculating a segment's derivative can't be part of segment_t directly. segment_t runs in the kernel and the only
/// places we use the derivative are in user-mode floating point. The floating-point derivative is encapsulated here.
/// This type also puts a name on the opaque ldexp calls necessary to transform the tangent between segment-local
/// parameter t space and spline-global spatial x space.
template <std::floating_point scalar_t> struct segment_derivative_t
{
    /// jacobian
    constexpr auto dx_dt(int_t log2_dx_dt) const noexcept -> scalar_t
    {
        using std::ldexp;
        return ldexp(scalar_t{1.0}, int_cast<int>(log2_dx_dt));
    }

    /// parametric derivative
    constexpr auto dy_dt(std::ranges::range auto coeffs, scalar_t t) const noexcept -> scalar_t
        requires(is_fixed<std::ranges::range_value_t<decltype(coeffs)>>)
    {
        return (scalar_t{3.0} * from_fixed<scalar_t>(coeffs[0]) * t + scalar_t{2.0} * from_fixed<scalar_t>(coeffs[1]))
            * t
            + from_fixed<scalar_t>(coeffs[2]);
    }

    /// spacial derivative
    constexpr auto dy_dx(std::ranges::range auto coeffs, scalar_t t, int_t log2_dx_dt) const noexcept -> scalar_t
        requires(is_fixed<std::ranges::range_value_t<decltype(coeffs)>>)
    {
        return dy_dt_to_dy_dx(dy_dt(coeffs, t), log2_dx_dt);
    }

    /// chain rule from t to x
    constexpr auto dy_dt_to_dy_dx(scalar_t dy_dt, int_t log2_dx_dt) const noexcept -> scalar_t
    {
        using std::ldexp;
        return ldexp(dy_dt, int_cast<int>(-log2_dx_dt));
    }

    // chain rule from x to t
    constexpr auto dy_dx_to_dy_dt(scalar_t dy_dx, int_t log2_dx_dt) const noexcept -> scalar_t
    {
        using std::ldexp;
        return ldexp(dy_dx, int_cast<int>(log2_dx_dt));
    }
};

} // namespace crv::spline
