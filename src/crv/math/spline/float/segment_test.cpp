// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment.hpp"
#include <crv/test/test.hpp>

namespace crv::spline::floating_point {
namespace {

struct spline_floating_point_segment_test_t : Test
{
    using real_t = float_t;
    using sut_t  = segment_t<real_t>;

    sut_t sut{1.2, -3.4, 5.6, 7.8};

    auto evaluate(real_t t) const noexcept -> real_t
    {
        return sut.coeffs[0] * t * t * t + sut.coeffs[1] * t * t + sut.coeffs[2] * t + sut.coeffs[3];
    }
};

TEST_F(spline_floating_point_segment_test_t, domain_min)
{
    auto const t        = real_t{0.0};
    auto const expected = evaluate(t);

    auto const actual = sut.evaluate(t);

    EXPECT_DOUBLE_EQ(expected, actual);
}

TEST_F(spline_floating_point_segment_test_t, domain_max)
{
    auto const t        = real_t{1.0};
    auto const expected = evaluate(t);

    auto const actual = sut.evaluate(t);

    EXPECT_DOUBLE_EQ(expected, actual);
}

TEST_F(spline_floating_point_segment_test_t, interior)
{
    auto const t        = real_t{0.327};
    auto const expected = evaluate(t);

    auto const actual = sut.evaluate(t);

    EXPECT_DOUBLE_EQ(expected, actual);
}

TEST_F(spline_floating_point_segment_test_t, before_domain)
{
    EXPECT_DEBUG_DEATH(sut.evaluate(-0.01), "in \\[0, 1\\]");
}

TEST_F(spline_floating_point_segment_test_t, after_domain)
{
    EXPECT_DEBUG_DEATH(sut.evaluate(1.01), "in \\[0, 1\\]");
}

} // namespace
} // namespace crv::spline::floating_point
