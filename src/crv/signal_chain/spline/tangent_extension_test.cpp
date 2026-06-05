// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "tangent_extension.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::spline {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// extended_tangent_t
// --------------------------------------------------------------------------------------------------------------------

namespace extended_tangent_tests {

using x_t = fixed_t<int64_t, 14>;
using y_t = fixed_t<int64_t, 25>;
using mantissa_t = int64_t;
using unpacked_field_t = unpacked_field_t<mantissa_t>;

using sut_t = extended_tangent_t<x_t, y_t, unpacked_field_t>;

constexpr auto x_inf = max<x_t>();

// constant slope
// y = 0.0*x + 3.0
static_assert(sut_t{.slope = {.mantissa = 0, .shift = 0}, .y0 = y_t{3}, .x_max_delta = x_inf}(x_t{5}) == y_t{3});
static_assert(sut_t{.slope = {.mantissa = 0, .shift = 0}, .y0 = y_t{3}, .x_max_delta = x_inf}(x_t{7}) == y_t{3});

// positive slope
// y = 1.0*x + 0.0
// required shift = 45 - 26 = 19
static_assert(
    sut_t{.slope = {.mantissa = 1LL << 30, .shift = 19}, .y0 = y_t{0}, .x_max_delta = x_inf}(x_t{5}) == y_t{5});
static_assert(
    sut_t{.slope = {.mantissa = 1LL << 30, .shift = 19}, .y0 = y_t{0}, .x_max_delta = x_inf}(x_t{7}) == y_t{7});

// negative slope with intercept
// y = -0.5*x + 4.0
// required shift = 45 - 25 = 20
static_assert(
    sut_t{.slope = {.mantissa = -(1LL << 30), .shift = 20}, .y0 = y_t{4}, .x_max_delta = x_inf}(x_t{2}) == y_t{3});
static_assert(
    sut_t{.slope = {.mantissa = -(1LL << 30), .shift = 20}, .y0 = y_t{4}, .x_max_delta = x_inf}(x_t{4}) == y_t{2});

// y = 1.0*x + 0.0, shift = 19. Clamp at x=5
static_assert(
    sut_t{.slope = {.mantissa = 1LL << 30, .shift = 19}, .y0 = y_t{0}, .x_max_delta = x_t{5}}(x_t{3}) == y_t{3});
static_assert(
    sut_t{.slope = {.mantissa = 1LL << 30, .shift = 19}, .y0 = y_t{0}, .x_max_delta = x_t{5}}(x_t{10}) == y_t{5});

} // namespace extended_tangent_tests

// --------------------------------------------------------------------------------------------------------------------
// tangent_extender_t
// --------------------------------------------------------------------------------------------------------------------

struct spline_tangent_extender_test_t : Test
{
    using scalar_t = float_t;
    using jet_t = jet_t<scalar_t>;
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
        using x_t = x_t;
        using y_t = y_t;
        using unpacked_field_t = unpacked_field_t;

        unpacked_field_t slope;
        y_t y0;
        x_t x_max_delta;

        constexpr auto operator==(extended_tangent_t const&) const noexcept -> bool = default;
    };

    struct float_extractor_t
    {
        using scalar_t = scalar_t;

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
        using x_t = x_t;
        constexpr auto operator()(x_t) const noexcept -> y_t { return y_t{37}; }
    };

    struct interval_t
    {
        using segment_t = segment_t;

        struct subdomain_t
        {
            int_t log2_width = 47;
        };
        subdomain_t subdomain;

        struct cubic_result_t
        {
            scalar_t y;
            scalar_t df;

            friend constexpr auto primal(cubic_result_t const& r) noexcept -> scalar_t { return r.y; }
            friend constexpr auto tangent(cubic_result_t const& r) noexcept -> scalar_t { return r.df; }
        };

        constexpr auto cubic(jet_t jet) const noexcept -> cubic_result_t { return {.y = 37.0, .df = jet.df}; }

        segment_t segment;
    };

    using sut_t = tangent_extender_t<interval_t, extended_tangent_t, float_extractor_t>;
    sut_t sut{.y_limit = 100.0, .extract_float = {}};

    interval_t interval{};
};

TEST_F(spline_tangent_extender_test_t, result)
{
    // dt_dx = 1.0 * 2^(49 - 47) = 4.0
    // extracted_slope = { .mantissa = 4, .exponent = -5 }
    // shift = 14 - 25 - (-5) = -6
    //
    // x_max_delta:
    // dy_dx = 4.0
    // y = 37.0
    // y_limit = 100.0
    // delta_x_real = (100.0 - 37.0) / 4.0 = 15.75

    auto const expected_delta = to_fixed<x_t>(15.75f);

    auto const expected
        = extended_tangent_t{.slope = {.mantissa = 4, .shift = -6}, .y0 = y_t{37}, .x_max_delta = expected_delta};

    auto const actual = sut(interval);

    EXPECT_EQ(expected, actual);
}

} // namespace
} // namespace crv::spline
