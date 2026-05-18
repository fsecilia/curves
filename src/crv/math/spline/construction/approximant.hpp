// SPDX-License-Identifier: MIT

/// \file
/// \brief segment approximant
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/polynomial.hpp>

namespace crv::spline {

template <std::floating_point t_scalar_t, is_fixed t_x_t> struct approximant_t
{
    using scalar_t = t_scalar_t;
    using x_t = t_x_t;

    cubic_t<scalar_t> cubic;
    x_t x0;
    int_t log2_width;

    /// \returns spline-global spatial coordinate y
    constexpr auto operator()(scalar_t x) const noexcept -> scalar_t
    {
        auto const x_local = from_fixed<scalar_t>(to_fixed<x_t>(x) - x0);
        auto const t = std::ldexp(x_local, int_cast<int>(-log2_width));
        return cubic(t);
    }
};

} // namespace crv::spline
