// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/rsqrt.hpp>
#include <crv/math/fixed/uabs.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/math/shifter.hpp>
#include <cassert>

namespace crv::pipeline {

/// fixed-point length of a Linux relative displacement pair
///
/// Squares fit uint64; the magnitude stays wide until velocity_t divides by report time.
template <typename rsqrt_t> struct displacement_magnitude_t
{
    using input_t = int32_t;
    using squared_t = fixed_t<uint64_t, 0>;
    using reciprocal_t = rsqrt_t::out_t;
    using out_t = fixed::product_t<squared_t, reciprocal_t>;

    [[no_unique_address]] rsqrt_t rsqrt{};

    constexpr auto operator()(input_t x, input_t y) const noexcept -> out_t
    {
        using axis_t = fixed_t<input_t, 0>;

        auto const x_magnitude = uabs(axis_t{x});
        auto const y_magnitude = uabs(axis_t{y});
        auto const squared = multiply(x_magnitude, x_magnitude) + multiply(y_magnitude, y_magnitude);

        if (squared == squared_t{}) return out_t{};

        return multiply(squared, rsqrt(squared));
    }
};

/// curve velocity from original report displacement
///
/// Scale is 1e9/DPI, converting counts/ns to 1000-DPI-equivalent counts/ms; both narrowings are checked before use.
template <is_fixed t_out_t, typename t_magnitude_t>
    requires(is_signed_v<t_out_t>)
struct velocity_t
{
    using out_t = t_out_t;
    using magnitude_t = t_magnitude_t;
    using input_t = magnitude_t::input_t;
    using duration_t = fixed_t<uint64_t, 0>;
    using wide_rate_t = fixed_t<uint128_t, 64>;
    using rate_t = fixed_t<uint64_t, 64>;
    using scale_t = fixed_t<uint64_t, 34>;

    struct result_t
    {
        out_t value{};
        bool valid = false;

        constexpr auto operator==(result_t const&) const noexcept -> bool = default;
    };

    [[no_unique_address]] magnitude_t magnitude{};

    constexpr auto operator()(input_t x, input_t y, duration_t duration, scale_t scale) const noexcept -> result_t
    {
        assert(duration > duration_t{} && "velocity_t: report duration must be positive");
        assert(scale > scale_t{} && "velocity_t: velocity scale must be positive");

        auto const wide_rate = divide<wide_rate_t>(magnitude(x, y), duration, rounding_modes::div::nearest_away);
        if (wide_rate > wide_rate_t::convert(max<rate_t>())) return {};

        auto const rate = rate_t::convert(wide_rate);
        auto const scaled = multiply(rate, scale);
        using scaled_t = decltype(scaled);

        static_assert(scaled_t::frac_bits >= out_t::frac_bits,
            "velocity_t: scaled rate must not require a left shift when converting to output");

        using wide_out_t = fixed_t<typename scaled_t::value_t, out_t::frac_bits>;
        static constexpr auto shifter = shifter_t<rounding_modes::shr::nearest_up>{};
        auto const wide_output = wide_out_t::template convert<shifter>(scaled);
        if (wide_output > wide_out_t::convert(max<out_t>())) return {};

        return {
            .value = out_t::convert(wide_output),
            .valid = true,
        };
    }
};

} // namespace crv::pipeline
