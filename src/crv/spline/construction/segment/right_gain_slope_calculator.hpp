// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/integer.hpp>
#include <cmath>
#include <concepts>

namespace crv::spline {

/// right gain slope of a segment
///
/// This is the derivative of induced gain at the right endpoint of a quantized segment.
template <std::floating_point t_scalar_t, is_fixed t_x_t, typename t_unpacked_segment_t>
struct right_gain_slope_calculator_t
{
    using scalar_t = t_scalar_t;
    using x_t = t_x_t;
    using unpacked_segment_t = t_unpacked_segment_t;
    using y_t = unpacked_segment_t::y_t;

    constexpr auto operator()(unpacked_segment_t const& unpacked_segment, x_t width, x_t x0) const noexcept -> scalar_t
    {
        assert(width > x_t{0});
        assert(x0 >= x_t{0});

        // interprets runtime shifts as cumulative Horner scales, not independent coefficient exponents
        auto const b_exponent = -unpacked_segment.b.shift - y_t::frac_bits;
        auto const c_exponent = x_t::frac_bits - unpacked_segment.c.shift + b_exponent;
        auto const d_exponent = x_t::frac_bits - unpacked_segment.d.shift + c_exponent;

        auto const b = scale(unpacked_segment.b.significand, b_exponent);
        auto const c = scale(unpacked_segment.c.significand, c_exponent);
        auto const d = scale(unpacked_segment.d.significand, d_exponent);
        auto const u = from_fixed<scalar_t>(width);
        auto const s_prime = c + scalar_t{2} * d * u;

        if (x0 == x_t{0}) return s_prime;

        auto const s = b + u * (c + u * d);
        auto const x0_scalar = from_fixed<scalar_t>(x0);
        auto const x = from_fixed<scalar_t>(x0 + width);
        auto const g0 = from_fixed<scalar_t>(unpacked_segment.g0);

        return (u / x) * s_prime + (x0_scalar / (x * x)) * (s - g0);
    }

private:
    static constexpr auto scale(auto significand, int_t exponent) noexcept -> scalar_t
    {
        using std::ldexp;
        return ldexp(static_cast<scalar_t>(significand), int_cast<int>(exponent));
    }
};

} // namespace crv::spline
