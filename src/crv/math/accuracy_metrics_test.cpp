// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include "accuracy_metrics.hpp"
#include <crv/test/test.hpp>
#include <gtest/gtest.h>

namespace crv {
namespace {

struct accuracy_metrics_test_t : Test
{
    using real_t        = float_t;
    using arg_t         = int_t;
    using accumulator_t = real_t;
    using sut_t         = accuracy_metrics_t<arg_t, real_t, accumulator_t>;

    sut_t sut;

    constexpr auto test(sut_t::error_stats_t const& expected, sut_t::error_stats_t const& actual) const noexcept -> void
    {
        EXPECT_DOUBLE_EQ(expected.sse, actual.sse);
        EXPECT_DOUBLE_EQ(expected.max, actual.max);
        EXPECT_EQ(expected.arg_max, actual.arg_max);
    }

    constexpr auto test(sut_t const& expected, sut_t const& actual) const noexcept -> void
    {
        EXPECT_EQ(expected.sample_count, actual.sample_count);
        test(expected.abs, actual.abs);
        test(expected.rel, actual.rel);
    }
};

TEST_F(accuracy_metrics_test_t, identity)
{
    // feed identical value
    constexpr auto sample_count = 100;
    for (auto i = 0; i < sample_count; ++i) sut.sample(i, 42.0, 42.0);

    test(sut_t{.sample_count = sample_count}, sut);
}

constexpr auto sqr(auto x) noexcept -> auto
{
    return x * x;
}

TEST_F(accuracy_metrics_test_t, known_sequence)
{
    // first sample sets new abs and rel arg_max
    sut.sample(3, 2.5, 2);
    test(sut_t{.sample_count = 1,
               .abs          = {.sse = 0.25, .max = 0.5, .arg_max = 3},
               .rel          = {.sse = 0.0625, .max = 0.25, .arg_max = 3}},
         sut);

    // new abs arg_max
    sut.sample(5, 5, 4);
    test(sut_t{.sample_count = 2,
               .abs          = {.sse = 1.25, .max = 1, .arg_max = 5},
               .rel          = {.sse = 0.125, .max = 0.25, .arg_max = 3}},
         sut);

    // new rel arg_max
    sut.sample(2, 0.1, 0.01);
    test(sut_t{.sample_count = 3,
               .abs          = {.sse = 1.2581, .max = 1, .arg_max = 5},
               .rel          = {.sse = 81.125, .max = 0.09 / 0.01, .arg_max = 2}},
         sut);

    EXPECT_EQ(1.2581 / 3, sut.abs_mse());
    EXPECT_EQ(std::sqrt(1.2581 / 3), sut.abs_rmse());
    EXPECT_EQ(81.125 / 3, sut.rel_mse());
    EXPECT_EQ(std::sqrt(81.125 / 3), sut.rel_rmse());
}

TEST_F(accuracy_metrics_test_t, rel_ignores_expected_zero)
{
    // sample_count and abs are still updated, but rel is ignored
    sut.sample(3, 10, 0);
    test(sut_t{.sample_count = 1,
               .abs          = {.sse = sqr(10), .max = 10, .arg_max = 3},
               .rel          = {.sse = 0, .max = 0, .arg_max = 0}},
         sut);
}

} // namespace
} // namespace crv
