// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "hermite_converter.hpp"
#include <crv/test/test.hpp>
#include <array>

namespace crv::spline {
namespace {

using real_t = float_t;
using jet_t = jet_t<real_t>;
using coeff_t = fixed_t<int32_t, 13>;
using cubic_polynomial_t = cubic_polynomial_t<coeff_t>;
using values_t = std::array<real_t, std::tuple_size_v<cubic_polynomial_t>>;

struct vector_t
{
    jet_t left;
    jet_t right;
    cubic_polynomial_t expected;

    vector_t(jet_t left, jet_t right, values_t values) noexcept
        : left{left}, right{right}, expected{to_fixed<coeff_t>(values[0]), to_fixed<coeff_t>(values[1]),
                                        to_fixed<coeff_t>(values[2]), to_fixed<coeff_t>(values[3])}
    {}
};

struct hermite_converter_test_t : TestWithParam<vector_t>
{
    jet_t const left = GetParam().left;
    jet_t const right = GetParam().right;
    cubic_polynomial_t const& expected = GetParam().expected;

    using sut_t = hermite_converter_t<coeff_t>;
    sut_t sut{};
};

TEST_P(hermite_converter_test_t, vector)
{
    auto const actual = sut(left, right);
    EXPECT_EQ(expected, actual);
}

vector_t const vectors[] = {
    // zero baseline
    {{0.0, 0.0}, {0.0, 0.0}, {0, 0, 0, 0}},

    //
    // single-input isolation
    //

    // p0 = 1.0 (dp = -1.0)
    // a = -2(-1) = 2, b = 3(-1) = -3, c = 0, d = 1
    {{1.0, 0.0}, {0.0, 0.0}, {2.0, -3.0, 0.0, 1.0}},

    // p1 = 1.0 (dp = 1.0)
    // a = -2(1) = -2, b = 3(1) = 3, c = 0, d = 0
    {{0.0, 0.0}, {1.0, 0.0}, {-2.0, 3.0, 0, 0}},

    // m0 = 1.0
    // a = 1, b = -2, c = 1, d = 0
    {{0.0, 1.0}, {0.0, 0.0}, {1.0, -2.0, 1.0, 0}},

    // m1 = 1.0
    // a = 1, b = -1, c = 0, d = 0
    {{0.0, 0.0}, {0.0, 1.0}, {1.0, -1.0, 0, 0}},

    //
    // combined inputs
    //

    // dp = 0, tangent-only: p0 = p1 = 1.0, m0 = 1.0, m1 = -1.0
    // a = 0 + 1 + (-1) = 0, b = 0 - 2 - (-1) = -1, c = 1, d = 1
    {{1.0, 1.0}, {1.0, -1.0}, {0, -1.0, 1.0, 1.0}},

    // negative dp with all inputs active: p0 = 2, m0 = 0.5, p1 = -1, m1 = -0.5 (dp = -3)
    // a = 6 + 0.5 + (-0.5) = 6, b = -9 - 1 - (-0.5) = -9.5, c = 0.5, d = 2
    {{2.0, 0.5}, {-1.0, -0.5}, {6.0, -9.5, 0.5, 2.0}},

    //
    // linear inputs (cubic and quadratic vanish)
    //

    // y = x: p0 = 0, m0 = 1, p1 = 1, m1 = 1
    {{0.0, 1.0}, {1.0, 1.0}, {0, 0, 1.0, 0}},

    // y = 2x + 1: p0 = 1, m0 = 2, p1 = 3, m1 = 2
    {{1.0, 2.0}, {3.0, 2.0}, {0, 0, 2.0, 1.0}},

    // mixed fractional and negative
    //
    // p0 = 0.5, m0 = -0.5, p1 = -0.5, m1 = 0.5 (dp = -1)
    // a = 2 + (-0.5) + 0.5 = 2, b = -3 + 1 - 0.5 = -2.5, c = -0.5, d = 0.5
    {{0.5, -0.5}, {-0.5, 0.5}, {2.0, -2.5, -0.5, 0.5}},

    // quantization: inputs not exactly representable in fixed<int32_t, 13>
    //
    // p0 = 1/3, p1 = 2/3, tangents = 0 (dp = 1/3)
    // a = -2/3, b = 1, c = 0, d = 1/3
    // 1/3 and 2/3 are not multiples of 1/8192; exercises to_fixed rounding
    {{1.0 / 3.0, 0.0}, {2.0 / 3.0, 0.0}, {-2.0 / 3.0, 1.0, 0.0, 1.0 / 3.0}},

    // large magnitude
    //
    // p0 = -50, m0 = 10, p1 = 50, m1 = -10 (dp = 100)
    // a = -200 + 10 + (-10) = -200, b = 300 - 20 + 10 = 290, c = 10, d = -50
    {{-50.0, 10.0}, {50.0, -10.0}, {-200.0, 290.0, 10.0, -50.0}},

    //
    // near max/min representable for fixed<int32_t, 13>
    // representable range: [-262144.0, 262143 + 8191/8192]
    // 3 * 87381 = 262143, exercising the 3x multiplier in b near positive max
    //

    // p1 = 87381 (dp = 87381)
    // a = -174762, b = 262143, c = 0, d = 0
    {{0.0, 0.0}, {87381.0, 0.0}, {-174762.0, 262143.0, 0.0, 0.0}},

    // p1 = -87381 (dp = -87381), symmetric near negative min
    // a = 174762, b = -262143, c = 0, d = 0
    {{0.0, 0.0}, {-87381.0, 0.0}, {174762.0, -262143.0, 0.0, 0.0}},
};
INSTANTIATE_TEST_SUITE_P(vectors, hermite_converter_test_t, ValuesIn(vectors));

} // namespace
} // namespace crv::spline
