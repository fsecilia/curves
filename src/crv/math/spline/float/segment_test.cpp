// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment.hpp"
#include <crv/math/limits.hpp>
#include <crv/math/spline/float/io.hpp>
#include <crv/test/test.hpp>
#include <limits>
#include <ostream>

namespace crv::spline::floating_point {
namespace {

using real_t = float_t;
using sut_t  = segment_t<real_t>;

auto const inf  = std::numeric_limits<real_t>::infinity();
auto const qnan = std::numeric_limits<real_t>::quiet_NaN();
auto const snan = std::numeric_limits<real_t>::signaling_NaN();

struct spline_floating_point_segment_test_t : Test
{
    constexpr auto evaluate(sut_t const& sut, real_t t) const noexcept -> real_t
    {
        return sut.coeffs[0] * t * t * t + sut.coeffs[1] * t * t + sut.coeffs[2] * t + sut.coeffs[3];
    }
};

struct spline_floating_point_segment_test_range_t : spline_floating_point_segment_test_t
{
    sut_t sut{1.2, -3.4, 5.6, 7.8};

    constexpr auto evaluate(real_t t) const noexcept -> real_t
    {
        return spline_floating_point_segment_test_t::evaluate(sut, t);
    }

    auto test(real_t t, real_t expected) const noexcept -> void
    {
        auto const actual = sut.evaluate(t);

        EXPECT_DOUBLE_EQ(expected, actual);
    }

    auto test(real_t t) const noexcept -> void { test(t, evaluate(t)); }
};

TEST_F(spline_floating_point_segment_test_range_t, domain_min)
{
    test(real_t{0.0});
}

TEST_F(spline_floating_point_segment_test_range_t, domain_max)
{
    test(real_t{1.0});
}

TEST_F(spline_floating_point_segment_test_range_t, interior)
{
    test(real_t{0.327});
}

TEST_F(spline_floating_point_segment_test_range_t, qnan)
{
    EXPECT_TRUE(std::isnan(sut.evaluate(qnan)));
}

TEST_F(spline_floating_point_segment_test_range_t, snan)
{
    EXPECT_TRUE(std::isnan(sut.evaluate(snan)));
}

TEST_F(spline_floating_point_segment_test_range_t, inf)
{
    EXPECT_TRUE(std::isinf(sut.evaluate(inf)));
}

// ====================================================================================================================
// Parameterized Test
// ====================================================================================================================

struct param_t
{
    sut_t  sut;
    real_t t;
    real_t expected;

    friend auto operator<<(std::ostream& out, param_t const& src) -> std::ostream&
    {
        return out << "{sut = " << src.sut << ", t = " << src.t << ", expected = " << src.expected << "}";
    }
};

struct spline_floating_point_segment_parameterized_test_t : spline_floating_point_segment_test_t,
                                                            WithParamInterface<param_t>
{
    sut_t const& sut      = GetParam().sut;
    real_t const t        = GetParam().t;
    real_t const expected = GetParam().expected;
};

TEST_P(spline_floating_point_segment_parameterized_test_t, expected_result)
{
    auto const actual = sut.evaluate(t);

    EXPECT_EQ(expected, actual);
};

param_t const params[] = {
    // all zeros
    {.sut = {0.0, 0.0, 0.0, 0.0}, .t = 0.0, .expected = 0.0},
    {.sut = {0.0, 0.0, 0.0, 0.0}, .t = 0.5, .expected = 0.0},
    {.sut = {0.0, 0.0, 0.0, 0.0}, .t = 1.0, .expected = 0.0},

    // simple cubic
    {.sut = {1.0, 0.0, 0.0, 0.0}, .t = 0.0, .expected = 0.0},
    {.sut = {1.0, 0.0, 0.0, 0.0}, .t = 0.5, .expected = 0.125},
    {.sut = {1.0, 0.0, 0.0, 0.0}, .t = 1.0, .expected = 1.0},

    // simple quadratic
    {.sut = {0.0, 1.0, 0.0, 0.0}, .t = 0.0, .expected = 0.0},
    {.sut = {0.0, 1.0, 0.0, 0.0}, .t = 0.5, .expected = 0.25},
    {.sut = {0.0, 1.0, 0.0, 0.0}, .t = 1.0, .expected = 1.0},

    // simple linear
    {.sut = {0.0, 0.0, 1.0, 0.0}, .t = 0.0, .expected = 0.0},
    {.sut = {0.0, 0.0, 1.0, 0.0}, .t = 0.5, .expected = 0.5},
    {.sut = {0.0, 0.0, 1.0, 0.0}, .t = 1.0, .expected = 1.0},

    // simple constant
    {.sut = {0.0, 0.0, 0.0, 1.0}, .t = 0.0, .expected = 1.0},
    {.sut = {0.0, 0.0, 0.0, 1.0}, .t = 0.5, .expected = 1.0},
    {.sut = {0.0, 0.0, 0.0, 1.0}, .t = 1.0, .expected = 1.0},

    // all 1s
    {.sut = {1.0, 1.0, 1.0, 1.0}, .t = 0.0, .expected = 1.0},
    {.sut = {1.0, 1.0, 1.0, 1.0}, .t = 0.5, .expected = 1.875},
    {.sut = {1.0, 1.0, 1.0, 1.0}, .t = 1.0, .expected = 4.0},

    // all -1s
    {.sut = {-1.0, -1.0, -1.0, -1.0}, .t = 0.0, .expected = -1.0},
    {.sut = {-1.0, -1.0, -1.0, -1.0}, .t = 0.5, .expected = -1.875},
    {.sut = {-1.0, -1.0, -1.0, -1.0}, .t = 1.0, .expected = -4.0},

    // mixed sign
    {.sut = {-1.0, -1.0, 1.0, -1.0}, .t = 0.0, .expected = -1.0},
    {.sut = {-1.0, -1.0, 1.0, -1.0}, .t = 0.5, .expected = -0.875},
    {.sut = {-1.0, -1.0, 1.0, -1.0}, .t = 1.0, .expected = -2.0},
};
INSTANTIATE_TEST_SUITE_P(params, spline_floating_point_segment_parameterized_test_t, ValuesIn(params));

} // namespace
} // namespace crv::spline::floating_point
