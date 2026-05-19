// SPDX-License-Identifier: MIT

/// \file
/// \brief segment tangent extension
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/spline/segment.hpp>
#include <climits>
#include <cmath>

namespace crv::spline {

template <typename t_x_t, typename t_y_t, typename t_unpacked_field_t,
    auto shifter = shifter_t<rounding_modes::shr::fast::nearest_up>{}>
struct extended_tangent_t
{
    using x_t = t_x_t;
    using y_t = t_y_t;
    using unpacked_field_t = t_unpacked_field_t;

    unpacked_field_t slope; // shift is exponent + x_t::frac_bits - y_t::frac_bits
    y_t y0;

    // \param x position relative to end of segment
    constexpr auto operator()(x_t x) const noexcept -> y_t
    {
        auto const wide_product = widen(slope.mantissa) * x.value;
        auto const narrow_product = shifter.template shr<typename y_t::value_t>(wide_product, slope.shift);
        return y_t::literal(add_wrap(narrow_product, y0.value));
    }
};

/// generates linear extension of final tangent of a segment
template <typename t_interval_t, typename t_segment_t, typename t_extended_tangent_t, typename float_extractor_t>
struct tangent_extender_t
{
    using interval_t = t_interval_t;
    using segment_t = t_segment_t;
    using extended_tangent_t = t_extended_tangent_t;

    using x_t = segment_t::x_t;
    using unpacked_field_t = extended_tangent_t::unpacked_field_t;
    using scalar_t = float_extractor_t::scalar_t;

    [[no_unique_address]] float_extractor_t extract_float;

    constexpr auto operator()(interval_t const& interval, segment_t const& segment) const noexcept -> extended_tangent_t
    {
        auto const log2_x_max
            = sizeof(typename x_t::value_t) * CHAR_BIT - x_t::frac_bits - is_signed_v<typename x_t::value_t>;

        // extract derivative
        auto const t = scalar_t{1};
        auto const dt_dx = std::ldexp(scalar_t{1}, int_cast<int>(log2_x_max - interval.subdomain.log2_width));
        auto const dy_dx_extended_segment = interval.cubic(jet_t{t, dt_dx}).df;
        auto const extracted_slope = extract_float(dy_dx_extended_segment);
        auto const required_shift = x_t::frac_bits - extended_tangent_t::y_t::frac_bits - extracted_slope.exponent;
        auto const slope
            = unpacked_field_t{.mantissa = extracted_slope.mantissa, .shift = int_cast<int_t>(required_shift)};

        // extract y intercept
        auto const segment_width = x_t{1} << interval.subdomain.log2_width;
        auto const y0 = segment(segment_width);

        return extended_tangent_t{.slope = slope, .y0 = y0};
    }
};

} // namespace crv::spline
