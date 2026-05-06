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

/// evaluates the derivative and converts between dy/dx and dy/dt using the jacobian, dx/dt
template <std::floating_point real_t> struct tangent_t
{
    /// jacobian
    constexpr auto dx_dt(int_t log2_dx_dt) const noexcept -> real_t
    {
        return std::ldexp(real_t{1.0}, int_cast<int>(log2_dx_dt));
    }

    /// parametric derivative
    constexpr auto dy_dt(std::ranges::range auto coeffs, real_t t) const noexcept -> real_t
        requires(is_fixed<std::ranges::range_value_t<decltype(coeffs)>>)
    {
        return (real_t{3.0} * from_fixed<real_t>(coeffs[0]) * t + real_t{2.0} * from_fixed<real_t>(coeffs[1])) * t
            + from_fixed<real_t>(coeffs[2]);
    }

    /// spacial derivative
    constexpr auto dy_dx(std::ranges::range auto coeffs, real_t t, int_t log2_dx_dt) const noexcept -> real_t
        requires(is_fixed<std::ranges::range_value_t<decltype(coeffs)>>)
    {
        return dy_dt_to_dy_dx(dy_dt(coeffs, t), log2_dx_dt);
    }

    /// chain rule from t to x
    constexpr auto dy_dt_to_dy_dx(real_t dy_dt, int_t log2_dx_dt) const noexcept -> real_t
    {
        return std::ldexp(dy_dt, int_cast<int>(-log2_dx_dt));
    }

    // chain rule from x to t
    constexpr auto dy_dx_to_dy_dt(real_t dy_dx, int_t log2_dx_dt) const noexcept -> real_t
    {
        return std::ldexp(dy_dx, int_cast<int>(log2_dx_dt));
    }
};

} // namespace crv::spline
