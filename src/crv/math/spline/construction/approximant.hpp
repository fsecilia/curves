// SPDX-License-Identifier: MIT

/// \file
/// \brief segment approximant
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/jet/jet.hpp>

namespace crv::spline {

/// adapts segment with float api, anchors to origin in fixed x, supplies derivative
template <std::floating_point t_real_t, typename segment_t, typename segment_derivative_t> struct approximant_t
{
    using real_t = t_real_t;
    using x_t = segment_t::x_t;

    x_t x0;
    segment_t segment;

    [[no_unique_address]] segment_derivative_t segment_derivative;

    /// \returns jet in spline-global spatial coordinates, {y, dy/dx}
    constexpr auto operator()(real_t x) const noexcept -> jet_t<real_t>
    {
        auto const t = segment.x_to_t(to_fixed<x_t>(x) - x0);
        auto const primal = from_fixed<real_t>(segment.evaluate(t));
        auto const tangent = segment_derivative.dy_dx(segment.coeffs(), t, segment.log2_width());
        return jet_t{primal, tangent};
    }
};

} // namespace crv::spline
