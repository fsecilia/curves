// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "tangent.hpp"
#include <crv/math/spline/polynomial.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

using real_t = float_t;
using coeff_t = fixed_t<int32_t, 18>;
using coeffs_t = cubic_polynomial_t<coeff_t>;

struct vector_t
{
    coeffs_t coeffs;
    int_t log2_width;
    real_t t;
    real_t expected_dx_dt;
    real_t expected_dy_dt;
    real_t expected_dy_dx;
};

struct tangent_test_t : TestWithParam<vector_t>
{
    using sut_t = tangent_t<real_t>;
    sut_t sut{};
};

TEST_P(tangent_test_t, dx_dt)
{
    EXPECT_NEAR(GetParam().expected_dx_dt, sut.dx_dt(GetParam().log2_width), 1e-10);
}

TEST_P(tangent_test_t, dy_dt)
{
    EXPECT_NEAR(GetParam().expected_dy_dt, sut.dy_dt(GetParam().coeffs, GetParam().t), 1e-10);
}

TEST_P(tangent_test_t, dy_dx)
{
    EXPECT_NEAR(GetParam().expected_dy_dx, sut.dy_dx(GetParam().coeffs, GetParam().t, GetParam().log2_width), 1e-10);
}

TEST_P(tangent_test_t, dy_dt_to_dy_dx)
{
    EXPECT_NEAR(GetParam().expected_dy_dx, sut.dy_dt_to_dy_dx(GetParam().expected_dy_dt, GetParam().log2_width), 1e-10);
}

TEST_P(tangent_test_t, dy_dx_to_dy_dt)
{
    EXPECT_NEAR(GetParam().expected_dy_dt, sut.dy_dx_to_dy_dt(GetParam().expected_dy_dx, GetParam().log2_width), 1e-10);
}

vector_t const vectors[] = {
    // pure cubic
    {
        .coeffs = {coeff_t{1}, coeff_t{0}, coeff_t{0}, coeff_t{0}},
        .log2_width = 0, // width = 1.0
        .t = 0.5,
        .expected_dx_dt = 1.0,
        .expected_dy_dt = 0.75,
        .expected_dy_dx = 0.75,
    },

    // pure quadratic
    {
        .coeffs = {coeff_t{0}, coeff_t{1}, coeff_t{0}, coeff_t{0}},
        .log2_width = -1, // width = 0.5
        .t = 1.0 / 3,
        .expected_dx_dt = 0.5,
        .expected_dy_dt = 2.0 / 3,
        .expected_dy_dx = 4.0 / 3,
    },

    // pure linear: slope is constant regardless of t
    {
        .coeffs = {coeff_t{0}, coeff_t{0}, to_fixed<coeff_t>(-2.5), coeff_t{0}},
        .log2_width = 1, // width = 2.0
        .t = 0.8,
        .expected_dx_dt = 2.0,
        .expected_dy_dt = -2.5,
        .expected_dy_dx = -1.25,
    },

    // flat line: derivative is exactly zero everywhere
    {
        .coeffs = {coeff_t{0}, coeff_t{0}, coeff_t{0}, coeff_t{38}},
        .log2_width = 2, // width = 4.0
        .t = 0.5,
        .expected_dx_dt = 4.0,
        .expected_dy_dt = 0.0,
        .expected_dy_dx = 0.0,
    },

    // degenerate: all values are 0
    {
        .coeffs = {coeff_t{0}, coeff_t{0}, coeff_t{0}, coeff_t{0}},
        .log2_width = 0, // width = 1.0
        .t = 0.0,
        .expected_dx_dt = 1.0,
        .expected_dy_dt = 0.0,
        .expected_dy_dx = 0.0,
    },

    // evaluation at t = 0
    {
        .coeffs = {coeff_t{2}, coeff_t{-3}, coeff_t{4}, coeff_t{1}},
        .log2_width = 0, // width = 1.0
        .t = 0.0,
        .expected_dx_dt = 1.0,
        .expected_dy_dt = 4.0, // only the linear term 'c' remains
        .expected_dy_dx = 4.0,
    },

    // evaluation at t = 1
    {
        .coeffs = {coeff_t{2}, coeff_t{-3}, coeff_t{4}, coeff_t{1}},
        .log2_width = 0, // width = 1.0
        .t = 1.0,
        .expected_dx_dt = 1.0,
        .expected_dy_dt = 4.0, // 3(2)(1) + 2(-3)(1) + 4 = 6 - 6 + 4
        .expected_dy_dx = 4.0,
    },

    // mixed coefficients with negative segment width exponent
    {
        // y(t) = 2t^3 - 4t^2 + 1.5t - 10
        .coeffs = {coeff_t{2}, coeff_t{-4}, to_fixed<coeff_t>(1.5), coeff_t{-10}},
        .log2_width = -2, // width = 0.25
        .t = 0.5,
        .expected_dx_dt = 0.25,
        .expected_dy_dt = -1.0, // 3(2)(0.25) + 2(-4)(0.5) + 1.5 = 1.5 - 4 + 1.5 = -1
        .expected_dy_dx = -4.0, // -1.0 / 0.25
    },

    // mixed coefficients with positive segment width exponent
    {
        // y(t) = -1t^3 + 2t^2 - 1t + 0
        .coeffs = {coeff_t{-1}, coeff_t{2}, coeff_t{-1}, coeff_t{0}},
        .log2_width = 3, // width = 8.0
        .t = 0.5,
        .expected_dx_dt = 8.0,
        .expected_dy_dt = 0.25, // 3(-1)(0.25) + 2(2)(0.5) - 1 = -0.75 + 2 - 1 = 0.25
        .expected_dy_dx = 0.03125, // 0.25 / 8.0
    }};
INSTANTIATE_TEST_SUITE_P(vectors, tangent_test_t, ValuesIn(vectors));

} // namespace
} // namespace crv::spline
