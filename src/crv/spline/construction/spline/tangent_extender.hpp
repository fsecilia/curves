// SPDX-License-Identifier: MIT

/// \file
/// \brief gain-space final spline tangent construction
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/spline/construction/segment/amr/interval.hpp>
#include <crv/spline/tangent_extension.hpp>
#include <cassert>

namespace crv::spline {

/// builds final gain-space tangent from final transfer endpoint
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
        auto const x_max = from_fixed<scalar_t>(interval.subdomain.right_x);
        assert(x_max > scalar_t{0});

        // derive endpoint transfer and gain slope
        //
        // The jet gives T(X) and T'(X), then G'(X) = (T'(X) - G(X)) / X.
        auto const transfer_jet = interval.subdomain.right.y;
        auto const transfer = primal(transfer_jet);
        auto const transfer_slope = tangent(transfer_jet);
        auto const gain = transfer / x_max;
        auto const gain_slope = (transfer_slope - gain) / x_max;

        // authored gain and sensitivity are nondecreasing, so target endpoint gain slope must also be nonnegative.
        assert(gain_slope >= scalar_t{0} && "tangent_extender_t: final gain slope must stay nonnegative");

        auto const extracted_slope = extract_float(gain_slope);
        auto const required_shift = x_t::frac_bits - y_t::frac_bits - extracted_slope.exponent;
        auto const slope
            = unpacked_field_t{.mantissa = extracted_slope.mantissa, .shift = int_cast<int_t>(required_shift)};

        // anchor extension to shipped fixed segment, not floating endpoint
        auto const y0 = interval.segment(interval.subdomain.right_x, interval.subdomain.left_x);
        auto const y_limit_fixed = to_fixed<y_t>(y_limit);
        auto const x_max_delta = extended_tangent_t::clamp_delta(slope, y0, y_limit_fixed);

        return extended_tangent_t{.slope = slope, .y0 = y0, .x_max_delta = x_max_delta};
    }
};

} // namespace crv::spline
