// SPDX-License-Identifier: MIT

/// \file
/// \brief segment tangent extension
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/spline/construction/segment/amr/interval.hpp>
#include <crv/spline/tangent_extension.hpp>
#include <climits>

namespace crv::spline {

/// generates linear extension of final tangent of a segment
template <typename t_interval_t, typename t_extended_tangent_t, typename float_extractor_t> struct tangent_extender_t
{
    using interval_t = t_interval_t;
    using extended_tangent_t = t_extended_tangent_t;

    using segment_t = interval_t::segment_t;
    using x_t = segment_t::x_t;
    using unpacked_field_t = extended_tangent_t::unpacked_field_t;
    using scalar_t = float_extractor_t::scalar_t;

    scalar_t y_limit;
    [[no_unique_address]] float_extractor_t extract_float;

    constexpr auto operator()(interval_t const& interval) const noexcept -> extended_tangent_t
    {
        auto const log2_x_max
            = sizeof(typename x_t::value_t) * CHAR_BIT - x_t::frac_bits - is_signed_v<typename x_t::value_t>;
        auto const log2_width = interval.subdomain.log2_width;

        // find jet at t=1
        auto const t = scalar_t{1};
        auto const dt_dx = std::ldexp(scalar_t{1}, int_cast<int>(log2_x_max - log2_width));
        auto const y_jet = interval.cubic(jet_t{t, dt_dx});

        // extract slope
        auto const dy_dx = tangent(y_jet);
        auto const extracted_slope = extract_float(dy_dx);
        auto const required_shift = x_t::frac_bits - extended_tangent_t::y_t::frac_bits - extracted_slope.exponent;
        auto const slope
            = unpacked_field_t{.mantissa = extracted_slope.mantissa, .shift = int_cast<int_t>(required_shift)};

        auto x_max_delta = max<x_t>();
        assert(dy_dx >= scalar_t{0});
        if (dy_dx > scalar_t{0})
        {
            auto const y = primal(y_jet);
            assert(y <= y_limit);
            auto const delta_x_real = (y_limit - y) / dy_dx;
            if (delta_x_real >= scalar_t{0})
            {
                // floor and convert manually instead of rounding to avoid overflow
                auto const scale = 1LL << x_t::frac_bits;
                auto const floored = std::floor(delta_x_real * scale);
                x_max_delta = min(x_max_delta, x_t::literal(static_cast<x_t::value_t>(floored)));
            }
        }

        // extract quantized y intercept
        auto const segment_width = x_t{1} << log2_width;
        auto const y0 = interval.segment(segment_width);

        return extended_tangent_t{.slope = slope, .y0 = y0, .x_max_delta = x_max_delta};
    }
};

} // namespace crv::spline
