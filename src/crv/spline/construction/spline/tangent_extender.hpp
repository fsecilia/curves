// SPDX-License-Identifier: MIT

/// \file
/// \brief gain-space final spline tangent construction
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/spline/construction/segment/amr/interval.hpp>
#include <crv/spline/tangent_extension.hpp>
#include <cassert>

namespace crv::spline {

/// builds final gain-space tangent from right gain slope and right endpoint of final mapped interval
template <typename t_interval_t, typename t_extended_tangent_t, typename float_extractor_t> struct tangent_extender_t
{
    using interval_t = t_interval_t;
    using extended_tangent_t = t_extended_tangent_t;
    using segment_t = interval_t::segment_t;
    using x_t = segment_t::x_t;
    using y_t = extended_tangent_t::y_t;
    using unpacked_field_t = extended_tangent_t::unpacked_field_t;
    using scalar_t = float_extractor_t::scalar_t;

    scalar_t y_limit;
    [[no_unique_address]] float_extractor_t extract_float;

    constexpr auto operator()(interval_t const& interval) const noexcept -> extended_tangent_t
    {
        auto const gain_slope = interval.right_gain_slope;
        assert(gain_slope >= scalar_t{0} && "tangent_extender_t: final gain slope must stay nonnegative");

        auto const extracted_slope = extract_float(gain_slope);
        auto const required_shift = x_t::frac_bits - y_t::frac_bits - extracted_slope.exponent;
        auto const slope
            = unpacked_field_t{.significand = extracted_slope.significand, .shift = int_cast<int_t>(required_shift)};

        // anchor extension to shipped fixed segment, not floating endpoint
        auto const y0 = interval.segment(interval.subdomain.right_x, interval.subdomain.left_x);
        auto const y_limit_fixed = to_fixed<y_t>(y_limit);
        auto const x_max_delta = extended_tangent_t::clamp_delta(slope, y0, y_limit_fixed);

        return extended_tangent_t{.slope = slope, .y0 = y0, .x_max_delta = x_max_delta};
    }
};

} // namespace crv::spline
