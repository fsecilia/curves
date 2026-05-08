// SPDX-License-Identifier: MIT

/// \file
/// \brief
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/jet/jet.hpp>

namespace crv::spline {

/// creates fixed segment from a pair of float hermite knots and log2_width; scales tangents by segment width
template <typename t_real_t, typename t_segment_t, typename segment_derivative_t, typename hermite_converter_t>
struct segment_factory_t
{
    using real_t = t_real_t;
    using segment_t = t_segment_t;

    using jet_t = jet_t<real_t>;

    [[no_unique_address]] segment_derivative_t segment_derivative;
    [[no_unique_address]] hermite_converter_t hermite_converter;

    constexpr auto operator()(jet_t left_y, jet_t right_y, int_t log2_width) const noexcept -> segment_t
    {
        // convert from spline-global dy/dx to segment-local dy/dt via chain rule
        auto const dx_dt = segment_derivative.dx_dt(log2_width);
        left_y.df *= dx_dt;
        right_y.df *= dx_dt;

        using packed_segment_t = segment_t::packed_segment_t;
        return segment_t{packed_segment_t{hermite_converter(left_y, right_y), int_cast<int8_t>(log2_width)}};
    }
};

} // namespace crv::spline
