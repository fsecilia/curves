// SPDX-License-Identifier: MIT

/// \file
/// \brief polynomial approximation of target function
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/polynomial.hpp>
#include <cassert>
#include <cmath>

namespace crv::spline {

/// approximates a segment with a cubic polynomial
template <std::floating_point t_scalar_t, is_fixed t_x_t> struct approximant_t
{
    using scalar_t = t_scalar_t;
    using x_t = t_x_t;

    using cubic_t = cubic_t<scalar_t>;

    cubic_t cubic;
    x_t x0;
    int_t log2_width;

    /// \returns spline-global spatial coordinate y
    constexpr auto operator()(scalar_t x) const noexcept -> scalar_t
    {
        auto const x_local = from_fixed<scalar_t>(to_fixed<x_t>(x) - x0);
        auto const t = std::ldexp(x_local, int_cast<int>(-log2_width));
        return cubic(t);
    }

    constexpr auto operator==(approximant_t const&) const noexcept -> bool = default;
};

template <typename t_approximant_t> struct approximant_factory_t
{
    using approximant_t = t_approximant_t;

    using cubic_t = approximant_t::cubic_t;
    using x_t = approximant_t::x_t;

    constexpr auto operator()(cubic_t const& cubic, x_t x0, int_t log2_width) const noexcept -> approximant_t
    {
        return {.cubic = cubic, .x0 = x0, .log2_width = log2_width};
    }
};

} // namespace crv::spline
