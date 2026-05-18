// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "approximant.hpp"
#include <crv/test/test.hpp>
#include <gtest/gtest.h>

namespace crv::spline {
namespace {

struct spline_approximant_test_t : Test
{
    using scalar_t = float_t;
    using x_t = fixed_t<int16_t, 2>;
    using sut_t = approximant_t<scalar_t, x_t>;
};

TEST_F(spline_approximant_test_t, evaluates_exact_cubic_with_scale_and_offset)
{
    // P(t) = 1.0t^2 + 3.0t + 2.0
    auto const cubic = cubic_t{0.0, 1.0, 3.0, 2.0};

    auto const x0 = x_t{1};
    auto const log2_width = 1; // width = 2.0

    sut_t const sut{cubic, x0, log2_width};

    // input x = 2.0
    auto const input_x = scalar_t{2.0};

    // x_local = 2.0 - 1.0 = 1.0
    // t = x_local/2^1 = 0.5
    // y = P(0.5) = 1.0(0.5)^2 + 3.0(0.5) + 2.0 = 3.75

    auto const expected = scalar_t{3.75};

    auto const actual = sut(input_x);

    EXPECT_DOUBLE_EQ(expected, actual);
}

TEST_F(spline_approximant_test_t, quantizes_floating_point_input_to_fixed_grid)
{
    // P(t) = t
    auto const cubic = cubic_t{0.0, 0.0, 1.0, 0.0};
    auto const x0 = x_t{0};
    auto const log2_width = 0; // width = 1.0, t = x

    sut_t const sut{cubic, x0, log2_width};

    // input x that falls exactly between fixed-point representations
    auto const off_grid_input = scalar_t{0.125};

    // expected output should match what happens if we passed exactly 0.0
    auto const expected = scalar_t{0.0};

    auto const actual = sut(off_grid_input);

    EXPECT_DOUBLE_EQ(expected, actual);
}

} // namespace
} // namespace crv::spline
