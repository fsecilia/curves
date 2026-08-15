// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "tangent_extender.hpp"
#include <crv/math/polynomial.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

struct spline_tangent_extender_test_t : Test
{
    using scalar_t = float_t;
    using x_t = fixed_t<int64_t, 14>;
    using y_t = fixed_t<int64_t, 25>;
    using mantissa_t = int64_t;

    struct unpacked_field_t
    {
        mantissa_t mantissa;
        int_t shift;
        constexpr auto operator==(unpacked_field_t const&) const noexcept -> bool = default;
    };

    struct extended_tangent_t
    {
        using x_t = spline_tangent_extender_test_t::x_t;
        using y_t = spline_tangent_extender_test_t::y_t;
        using unpacked_field_t = spline_tangent_extender_test_t::unpacked_field_t;

        unpacked_field_t slope;
        y_t y0;
        x_t x_max_delta;

        constexpr auto operator==(extended_tangent_t const&) const noexcept -> bool = default;
    };

    struct float_extractor_t
    {
        using scalar_t = spline_tangent_extender_test_t::scalar_t;

        struct scaled_int_t
        {
            mantissa_t mantissa;
            int_t exponent;
        };

        constexpr auto operator()(scalar_t scalar) const noexcept -> scaled_int_t
        {
            return {.mantissa = static_cast<mantissa_t>(scalar), .exponent = -5};
        }
    };

    struct segment_t
    {
        using x_t = spline_tangent_extender_test_t::x_t;
        constexpr auto operator()(x_t) const noexcept -> y_t { return y_t{45}; }
    };

    struct subdomain_t
    {
        x_t width_ = x_t{2};
        constexpr auto width() const noexcept -> x_t { return width_; }
    };

    struct interval_t
    {
        using segment_t = spline_tangent_extender_test_t::segment_t;

        cubic_t<scalar_t> cubic{0.0, 0.0, 4.0, 37.0}; // T(u) = 4u + 37
        segment_t segment;
        subdomain_t subdomain;
    };

    using sut_t = tangent_extender_t<interval_t, extended_tangent_t, float_extractor_t>;
    sut_t sut{.y_limit = 100.0, .extract_float = {}};
    interval_t interval{};
};

TEST_F(spline_tangent_extender_test_t, uses_local_coordinate_transfer_derivative_without_extra_width_scaling)
{
    // At u = 2, T = 45 and dT/dx = dT/du = 4.
    // extracted_slope = {.mantissa = 4, .exponent = -5}
    // packed slope shift = 14 - 25 - (-5) = -6
    // extension limit = (100 - 45) / 4 = 13.75
    auto const expected = extended_tangent_t{
        .slope = {.mantissa = 4, .shift = -6},
        .y0 = y_t{45},
        .x_max_delta = to_fixed<x_t>(13.75),
    };

    EXPECT_EQ(expected, sut(interval));
}

} // namespace
} // namespace crv::spline
