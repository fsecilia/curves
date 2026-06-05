// SPDX-License-Identifier: MIT

/// \file
/// \brief segment tangent extension
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/shifter.hpp>

namespace crv::spline {

/// linear extension of final tangent of a segment
///
/// We spline a finite domain that is much smaller than the total input domain. For valid inputs beyond the end of the
/// domain, we extend the final tangent of the final segment.
template <typename t_x_t, typename t_y_t, typename t_unpacked_field_t,
    auto shifter = shifter_t<rounding_modes::shr::fast::nearest_up>{}>
struct extended_tangent_t
{
    using x_t = t_x_t;
    using y_t = t_y_t;
    using unpacked_field_t = t_unpacked_field_t;

    unpacked_field_t slope; // shift is exponent + x_t::frac_bits - y_t::frac_bits
    y_t y0;
    x_t x_max_delta; // clips tgangent to y_max

    // \param x position relative to end of segment
    constexpr auto operator()(x_t x) const noexcept -> y_t
    {
        auto const x_bounded = min(x.value, x_max_delta.value);

        auto const wide_product = widen(slope.mantissa) * x_bounded;
        auto const narrow_product = shifter.template shr<typename y_t::value_t>(wide_product, slope.shift);
        return y_t::literal(add_wrap(narrow_product, y0.value));
    }
};

} // namespace crv::spline
