// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include "accuracy_metrics.hpp"
#include <crv/test/test.hpp>
#include <gtest/gtest.h>
#include <sstream>

namespace crv {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// Arg Max
// --------------------------------------------------------------------------------------------------------------------

struct arg_max_test_t : Test
{
    using arg_t  = int_t;
    using real_t = float_t;
    using sut_t  = arg_max_t<int_t, real_t>;

    static constexpr auto old_max     = 3.0;
    static constexpr auto old_arg_max = 5;
    static constexpr auto new_arg     = 10;

    sut_t sut{.max = old_max, .arg_max = old_arg_max};
};

TEST_F(arg_max_test_t, initializes_to_min)
{
    ASSERT_EQ(min<real_t>(), sut_t{}.max);
}

TEST_F(arg_max_test_t, sample_without_new_max)
{
    auto const value = old_max - 1;
    sut.sample(new_arg, value);

    ASSERT_EQ(old_max, sut.max);
    ASSERT_EQ(old_arg_max, sut.arg_max);
}

TEST_F(arg_max_test_t, first_wins)
{
    auto const value = old_max;
    sut.sample(new_arg, value);

    ASSERT_EQ(old_max, sut.max);
    ASSERT_EQ(old_arg_max, sut.arg_max);
}

TEST_F(arg_max_test_t, sample_new_max)
{
    auto const new_max = old_max + 1;
    sut.sample(new_arg, new_max);

    ASSERT_EQ(new_max, sut.max);
    ASSERT_EQ(new_arg, sut.arg_max);
}

TEST_F(arg_max_test_t, ostream_inserter)
{
    auto expected = std::ostringstream{};
    expected << old_max << "@" << old_arg_max;

    auto actual = std::ostringstream{};
    actual << sut;

    ASSERT_EQ(expected.str(), actual.str());
}

// --------------------------------------------------------------------------------------------------------------------
// Arg Min
// --------------------------------------------------------------------------------------------------------------------

struct arg_min_test_t : Test
{
    using arg_t  = int_t;
    using real_t = float_t;
    using sut_t  = arg_min_t<int_t, real_t>;

    static constexpr auto old_min     = 3.0;
    static constexpr auto old_arg_min = 5;
    static constexpr auto new_arg     = 10;

    sut_t sut{.min = old_min, .arg_min = old_arg_min};
};

TEST_F(arg_min_test_t, initializes_to_max)
{
    ASSERT_EQ(max<real_t>(), sut_t{}.min);
}

TEST_F(arg_min_test_t, sample_without_new_min)
{
    auto const value = old_min + 1;
    sut.sample(new_arg, value);

    ASSERT_EQ(old_min, sut.min);
    ASSERT_EQ(old_arg_min, sut.arg_min);
}

TEST_F(arg_min_test_t, first_wins)
{
    auto const value = old_min;
    sut.sample(new_arg, value);

    ASSERT_EQ(old_min, sut.min);
    ASSERT_EQ(old_arg_min, sut.arg_min);
}

TEST_F(arg_min_test_t, sample_new_min)
{
    auto const new_min = old_min - 1;
    sut.sample(new_arg, new_min);

    ASSERT_EQ(new_min, sut.min);
    ASSERT_EQ(new_arg, sut.arg_min);
}

TEST_F(arg_min_test_t, ostream_inserter)
{
    auto expected = std::ostringstream{};
    expected << old_min << "@" << old_arg_min;

    auto actual = std::ostringstream{};
    actual << sut;

    ASSERT_EQ(expected.str(), actual.str());
}

// --------------------------------------------------------------------------------------------------------------------
// Min Max
// --------------------------------------------------------------------------------------------------------------------

struct min_max_test_t : Test
{
    using arg_t  = int_t;
    using real_t = float_t;

    static constexpr arg_t  arg_min     = 3;
    static constexpr real_t min         = 1.1;
    static constexpr arg_t  arg_max     = 5;
    static constexpr real_t max         = 1.2;
    static constexpr arg_t  arg_max_mag = arg_max;
    static constexpr real_t max_mag     = 1.2;

    struct arg_min_t
    {
        arg_t  arg_min{min_max_test_t::arg_min};
        real_t min{min_max_test_t::min};

        constexpr auto sample(arg_t arg, real_t min) noexcept -> void
        {
            arg_min   = arg;
            this->min = min;
        }

        friend auto operator<<(std::ostream& out, arg_min_t const&) -> std::ostream& { return out << "arg_min"; }
    };

    struct arg_max_t
    {
        arg_t  arg_max{min_max_test_t::arg_max};
        real_t max{min_max_test_t::max};

        constexpr auto sample(arg_t arg, real_t max) noexcept -> void
        {
            arg_max   = arg;
            this->max = max;
        }

        friend auto operator<<(std::ostream& out, arg_max_t const&) -> std::ostream& { return out << "arg_max"; }
    };

    using sut_t = min_max_t<arg_t, real_t, arg_min_t, arg_max_t>;
    sut_t sut{};
};

TEST_F(min_max_test_t, max_mag)
{
    ASSERT_EQ(max_mag, sut.max_mag());
}

TEST_F(min_max_test_t, arg_max_mag)
{
    ASSERT_EQ(arg_max_mag, sut.arg_max_mag());
}

TEST_F(min_max_test_t, sample)
{
    static constexpr auto sample_arg   = real_t{19.0};
    static constexpr auto sample_value = real_t{17.0};

    sut.sample(sample_arg, sample_value);

    EXPECT_EQ(sample_arg, sut.max_signed.arg_max);
    EXPECT_EQ(sample_value, sut.max_signed.max);
    EXPECT_EQ(sample_arg, sut.min_signed.arg_min);
    EXPECT_EQ(sample_value, sut.min_signed.min);
}

TEST_F(min_max_test_t, ostream_inserter)
{
    auto actual = std::ostringstream{};
    actual << sut;

    ASSERT_EQ("min = arg_min\nmax = arg_max", actual.str());
}

// --------------------------------------------------------------------------------------------------------------------
// Error Accumulator
// --------------------------------------------------------------------------------------------------------------------

struct error_accumulator_test_t : Test
{
    using arg_t         = int_t;
    using real_t        = float_t;
    using accumulator_t = float_t;

    static constexpr auto arg          = arg_t{3};
    static constexpr auto error        = real_t{15.2};
    static constexpr auto sample_count = int_t{11};

    static constexpr auto rmse = real_t{23.6};
    static constexpr auto mse  = rmse * rmse;
    static constexpr auto sse  = mse * sample_count;

    static constexpr auto bias     = 7.3;
    static constexpr auto variance = mse - bias * bias;
    static constexpr auto sum      = bias * sample_count;

    struct min_max_t
    {
        arg_t  arg{};
        real_t error{};

        constexpr auto sample(arg_t arg, real_t error) noexcept -> void
        {
            this->arg   = arg;
            this->error = error;
        }

        friend auto operator<<(std::ostream& out, min_max_t const&) -> std::ostream& { return out << "min_max"; }
    };

    using sut_t = error_accumulator_t<arg_t, real_t, accumulator_t, min_max_t>;
    sut_t sut{.sse = sse, .sum = sum, .min_max = {}, .sample_count = sample_count};
};

TEST_F(error_accumulator_test_t, sample)
{
    sut.sample(arg, error);

    EXPECT_EQ(sample_count + 1, sut.sample_count);
    EXPECT_EQ(sse + error * error, sut.sse);
    EXPECT_EQ(sum + error, sut.sum);
    EXPECT_EQ(arg, sut.min_max.arg);
    EXPECT_EQ(error, sut.min_max.error);
}

TEST_F(error_accumulator_test_t, mse)
{
    EXPECT_EQ(mse, sut.mse());
}

TEST_F(error_accumulator_test_t, rmse)
{
    EXPECT_EQ(rmse, sut.rmse());
}

TEST_F(error_accumulator_test_t, bias)
{
    EXPECT_EQ(bias, sut.bias());
}

TEST_F(error_accumulator_test_t, variance)
{
    EXPECT_EQ(variance, sut.variance());
}

TEST_F(error_accumulator_test_t, ostream_inserter)
{
    auto expected = std::ostringstream{};
    expected << "sample count = " << sample_count << "\nmin_max\nsum = " << sum << "\nmse = " << mse
             << "\nrmse = " << rmse << "\nbias = " << bias << "\nvariance = " << variance;

    auto actual = std::ostringstream{};
    actual << sut;

    ASSERT_EQ(expected.str(), actual.str());
}

// --------------------------------------------------------------------------------------------------------------------
// Accuracy Metrics
// --------------------------------------------------------------------------------------------------------------------

#if 0
struct accuracy_metrics_test_t : Test
{
    using real_t        = float_t;
    using arg_t         = int_t;
    using accumulator_t = real_t;
    using error_stats_t = error_stats_t<arg_t, real_t, accumulator_t>;
    using sut_t         = accuracy_metrics_t<arg_t, real_t, error_stats_t>;

    sut_t sut;

    constexpr auto test(error_stats_t const& expected, error_stats_t const& actual) const noexcept -> void
    {
        EXPECT_EQ(expected.sample_count, actual.sample_count);
        EXPECT_DOUBLE_EQ(expected.sse, actual.sse);
        EXPECT_DOUBLE_EQ(expected.max, actual.max);
        EXPECT_EQ(expected.arg_max, actual.arg_max);
    }

    constexpr auto test(sut_t const& expected, sut_t const& actual) const noexcept -> void
    {
        test(expected.abs, actual.abs);
        test(expected.rel, actual.rel);
    }
};

TEST_F(accuracy_metrics_test_t, identity)
{
    // feed identical value
    constexpr auto sample_count = 100;
    for (auto i = 0; i < sample_count; ++i) sut.sample(i, 42.0, 42.0);

    auto const expected_error_stats = error_stats_t{.sample_count = sample_count};
    test(sut_t{.abs = expected_error_stats, .rel = expected_error_stats}, sut);
}

constexpr auto sqr(auto x) noexcept -> auto
{
    return x * x;
}

TEST_F(accuracy_metrics_test_t, known_sequence)
{
    // first sample sets new abs and rel arg_max
    sut.sample(3, 2.5, 2);
    test(sut_t{.abs = {.sample_count = 1, .sse = 0.25, .max = 0.5, .arg_max = 3},
               .rel = {.sample_count = 1, .sse = 0.0625, .max = 0.25, .arg_max = 3}},
         sut);

    // new abs arg_max
    sut.sample(5, 5, 4);
    test(sut_t{.abs = {.sample_count = 2, .sse = 1.25, .max = 1, .arg_max = 5},
               .rel = {.sample_count = 2, .sse = 0.125, .max = 0.25, .arg_max = 3}},
         sut);

    // new rel arg_max
    sut.sample(2, 0.1, 0.01);
    test(sut_t{.abs = {.sample_count = 3, .sse = 1.2581, .max = 1, .arg_max = 5},
               .rel = {.sample_count = 3, .sse = 81.125, .max = 0.09 / 0.01, .arg_max = 2}},
         sut);

    EXPECT_EQ(1.2581 / 3, sut.abs_mse());
    EXPECT_EQ(std::sqrt(1.2581 / 3), sut.abs_rmse());
    EXPECT_EQ(81.125 / 3, sut.rel_mse());
    EXPECT_EQ(std::sqrt(81.125 / 3), sut.rel_rmse());
}

TEST_F(accuracy_metrics_test_t, rel_ignores_expected_zero)
{
    // abs sample_count is updated, but rel is ignored
    sut.sample(3, 10, 0);
    test(sut_t{.abs = {.sample_count = 1, .sse = sqr(10), .max = 10, .arg_max = 3},
               .rel = {.sample_count = 0, .sse = 0, .max = 0, .arg_max = 0}},
         sut);
}
#endif

} // namespace
} // namespace crv
