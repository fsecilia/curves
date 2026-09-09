// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "final_endpoint_acceptance.hpp"
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

struct final_endpoint_acceptance_test_t : Test
{
    using y_t = fixed_t<int64_t, 8>;
    using scalar_t = float_t;
    using sut_t = final_endpoint_acceptance_t<y_t, scalar_t>;

    static constexpr auto sut = sut_t{};
    static constexpr auto y_limit = y_t{1000};
};

TEST_F(final_endpoint_acceptance_test_t, accepts_anchor_in_range_with_positive_slope)
{
    EXPECT_TRUE(sut(y_t{500}, scalar_t{1}, y_limit));
}

TEST_F(final_endpoint_acceptance_test_t, accepts_zero_slope)
{
    EXPECT_TRUE(sut(y_t{500}, scalar_t{0}, y_limit));
}

TEST_F(final_endpoint_acceptance_test_t, accepts_zero_anchor)
{
    EXPECT_TRUE(sut(y_t{0}, scalar_t{1}, y_limit));
}

TEST_F(final_endpoint_acceptance_test_t, accepts_anchor_at_y_limit)
{
    EXPECT_TRUE(sut(y_limit, scalar_t{1}, y_limit));
}

TEST_F(final_endpoint_acceptance_test_t, rejects_negative_slope)
{
    EXPECT_FALSE(sut(y_t{500}, scalar_t{-1}, y_limit));
}

TEST_F(final_endpoint_acceptance_test_t, rejects_anchor_below_zero)
{
    EXPECT_FALSE(sut(y_t{-1}, scalar_t{1}, y_limit));
}

TEST_F(final_endpoint_acceptance_test_t, rejects_anchor_above_y_limit)
{
    EXPECT_FALSE(sut(y_t{1001}, scalar_t{1}, y_limit));
}

} // namespace
} // namespace crv::spline
