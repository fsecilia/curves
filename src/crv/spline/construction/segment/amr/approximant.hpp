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

namespace crv::spline {

/// approximates a segment with a cubic polynomial
template <std::floating_point t_scalar_t, typename t_segment_t> struct approximant_t
{
    using scalar_t = t_scalar_t;
    using segment_t = t_segment_t;

    using x_t = segment_t::x_t;

    segment_t segment;
    x_t x0;

    /// \returns spline-global spatial coordinate y in float, routing through fixed segment
    constexpr auto operator()(scalar_t x) const noexcept -> scalar_t
    {
        return from_fixed<scalar_t>(segment(to_fixed<x_t>(x) - x0));
    }

    constexpr auto operator==(approximant_t const&) const noexcept -> bool = default;
};

template <typename t_approximant_t> struct approximant_factory_t
{
    using approximant_t = t_approximant_t;

    using segment_t = approximant_t::segment_t;
    using x_t = approximant_t::x_t;

    constexpr auto operator()(segment_t const& segment, x_t x0) const noexcept -> approximant_t
    {
        return {.segment = segment, .x0 = x0};
    }
};

} // namespace crv::spline
