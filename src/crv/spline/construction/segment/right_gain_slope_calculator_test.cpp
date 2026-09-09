// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "right_gain_slope_calculator.hpp"
#include <crv/spline/segment.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

struct right_gain_slope_calculator_test_t : Test
{
    using scalar_t = float_t;
    using x_t = fixed_t<int64_t, 2>;
    using y_t = fixed_t<int64_t, 3>;
    using traits_t = spline::traits_t<spline::unpacked_field_t<int64_t>, y_t>;
    using unpacked_segment_t = traits_t::unpacked_segment_t;
    using unpacked_field_t = traits_t::unpacked_field_t;
    using sut_t = right_gain_slope_calculator_t<scalar_t, x_t, unpacked_segment_t>;

    sut_t sut;

    static constexpr auto create_field(int64_t significand, int_t shift) noexcept -> unpacked_field_t
    {
        return {.significand = significand, .shift = shift};
    }

    static constexpr auto create_segment(
        unpacked_field_t d, unpacked_field_t c, unpacked_field_t b, y_t g0 = y_t{0}) noexcept -> unpacked_segment_t
    {
        return {.d = d, .c = c, .b = b, .g0 = g0};
    }
};

TEST_F(right_gain_slope_calculator_test_t, origin_uses_linear_and_quadratic_slope_terms)
{
    auto const segment = create_segment(create_field(1, 0), create_field(2, 0), create_field(8, 0));

    EXPECT_EQ(sut(segment, x_t::literal(2), x_t{0}), 3.0);
}

TEST_F(right_gain_slope_calculator_test_t, nonzero_origin_applies_gain_correction)
{
    auto const segment = create_segment(create_field(1, 0), create_field(2, 0), create_field(8, 0), y_t::literal(8));

    auto const actual = sut(segment, x_t::literal(2), x_t::literal(6));

    EXPECT_EQ(actual, 1.125);
    EXPECT_NE(actual, 3.0);
}

TEST_F(right_gain_slope_calculator_test_t, interprets_relative_shifts_cumulatively)
{
    auto const segment = create_segment(create_field(2, 1), create_field(4, 2), create_field(8, -1));

    EXPECT_EQ(sut(segment, x_t::literal(2), x_t{0}), 2.0);
}

TEST_F(right_gain_slope_calculator_test_t, uses_quantized_gain_anchor)
{
    auto const segemnt = create_segment(create_field(0, 0), create_field(0, 0), create_field(8, -1), y_t::literal(6));

    auto const actual = sut(segemnt, x_t::literal(2), x_t::literal(6));

    EXPECT_EQ(actual, 0.46875);
    EXPECT_NE(actual, 0.45); // would result from an unquantized g0 of 0.8
}

TEST_F(right_gain_slope_calculator_test_t, constant_represented_gain_has_zero_slope)
{
    auto const segment = create_segment(create_field(0, 0), create_field(0, 0), create_field(8, -1), y_t::literal(16));

    EXPECT_EQ(sut(segment, x_t::literal(2), x_t::literal(6)), 0.0);
}

TEST_F(right_gain_slope_calculator_test_t, preserves_negative_slope)
{
    auto const segment = create_segment(create_field(0, 0), create_field(-2, 0), create_field(8, 0));

    EXPECT_EQ(sut(segment, x_t::literal(2), x_t{0}), -1.0);
}

} // namespace
} // namespace crv::spline
