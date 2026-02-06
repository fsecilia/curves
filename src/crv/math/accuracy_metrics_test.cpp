// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include "accuracy_metrics.hpp"
#include <crv/math/fixed/conversions.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/test/test.hpp>
#include <sstream>
#include <string_view>

namespace crv {
namespace {

// ====================================================================================================================
// Arg Max
// ====================================================================================================================

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

// ====================================================================================================================
// Arg Min
// ====================================================================================================================

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

// ====================================================================================================================
// Min Max
// ====================================================================================================================

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

// ====================================================================================================================
// Error Accumulator
// ====================================================================================================================

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

// ====================================================================================================================
// Metric Policies
// ====================================================================================================================

struct metric_policy_test_t : Test
{
    using arg_t   = int_t;
    using real_t  = float_t;
    using fixed_t = fixed_t<int64_t, 32>;

    static constexpr auto arg      = arg_t{372};
    static constexpr auto error    = real_t{1.0};
    static constexpr auto expected = real_t{4.0};

    struct error_accumulator_t
    {
        using arg_t  = arg_t;
        using real_t = real_t;

        arg_t  arg{};
        real_t error{};

        auto sample(arg_t arg, real_t error) noexcept -> void
        {
            this->arg   = arg;
            this->error = error;
        }
    };

    error_accumulator_t error_accumulator{};
};

// --------------------------------------------------------------------------------------------------------------------
// Metric Policy Diff
// --------------------------------------------------------------------------------------------------------------------

struct metric_policy_test_diff_t : metric_policy_test_t
{};

TEST_F(metric_policy_test_diff_t, positive)
{
    auto const sut = metric_policy::diff_t{};

    auto const actual = to_fixed<fixed_t>(expected + error);
    sut(error_accumulator, arg, actual, expected);

    EXPECT_EQ(arg, error_accumulator.arg);
    EXPECT_DOUBLE_EQ(error, error_accumulator.error);
}

TEST_F(metric_policy_test_diff_t, negative)
{
    auto const sut = metric_policy::diff_t{};

    auto const actual = to_fixed<fixed_t>(expected - error);
    sut(error_accumulator, arg, actual, expected);

    EXPECT_EQ(arg, error_accumulator.arg);
    EXPECT_DOUBLE_EQ(error, -error_accumulator.error);
}

// --------------------------------------------------------------------------------------------------------------------
// Metric Policy Rel
// --------------------------------------------------------------------------------------------------------------------

struct metric_policy_test_rel_t : metric_policy_test_t
{};

TEST_F(metric_policy_test_rel_t, zero)
{
    auto const sut = metric_policy::rel_t{};

    sut(error_accumulator, arg, fixed_t{1}, 0);

    EXPECT_EQ(0, error_accumulator.arg);
    EXPECT_EQ(0, error_accumulator.error);
}

TEST_F(metric_policy_test_rel_t, nonzero)
{
    auto const sut = metric_policy::rel_t{};

    auto const actual = to_fixed<fixed_t>(error * expected + expected);
    sut(error_accumulator, arg, actual, expected);

    EXPECT_EQ(arg, error_accumulator.arg);
    EXPECT_DOUBLE_EQ(error, error_accumulator.error);
}

// --------------------------------------------------------------------------------------------------------------------
// Metric Policy Ulps
// --------------------------------------------------------------------------------------------------------------------

struct metric_policy_test_ulps_t : metric_policy_test_t
{};

TEST_F(metric_policy_test_ulps_t, zero_error)
{
    auto const sut = metric_policy::ulps_t{};

    auto const ideal  = std::round(std::ldexp(expected, fixed_t::frac_bits));
    auto const actual = fixed_t{static_cast<fixed_t::value_t>(ideal)};
    sut(error_accumulator, arg, actual, expected);

    EXPECT_EQ(arg, error_accumulator.arg);
    EXPECT_DOUBLE_EQ(0.0, error_accumulator.error);
}

TEST_F(metric_policy_test_ulps_t, one_high)
{
    auto const sut = metric_policy::ulps_t{};

    auto const ideal  = std::round(std::ldexp(expected, fixed_t::frac_bits));
    auto const actual = fixed_t{static_cast<fixed_t::value_t>(error + ideal)};
    sut(error_accumulator, arg, actual, expected);

    EXPECT_EQ(arg, error_accumulator.arg);
    EXPECT_DOUBLE_EQ(error, error_accumulator.error);
}

TEST_F(metric_policy_test_ulps_t, ten_low)
{
    auto const sut = metric_policy::ulps_t{};

    auto const ideal  = std::round(std::ldexp(expected, fixed_t::frac_bits));
    auto const actual = fixed_t{static_cast<fixed_t::value_t>(-10 * error + ideal)};
    sut(error_accumulator, arg, actual, expected);

    EXPECT_EQ(arg, error_accumulator.arg);
    EXPECT_DOUBLE_EQ(-10 * error, error_accumulator.error);
}

// ====================================================================================================================
// Error Metric
// ====================================================================================================================

struct error_metric_test_t : Test
{
    struct error_accumulator_t
    {
        int_t id{};

        friend auto operator<<(std::ostream& out, error_accumulator_t const& src) -> std::ostream&
        {
            return out << "error_accumulator " << src.id;
        }

        auto operator==(error_accumulator_t const&) const noexcept -> bool = default;
    };

    using arg_t   = int_t;
    using fixed_t = int_t;
    using real_t  = int_t;

    struct policy_t
    {
        error_accumulator_t error_accumulator{};
        arg_t               arg{};
        fixed_t             actual{};
        real_t              expected{};
        auto                operator()(error_accumulator_t const& error_accumulator, arg_t arg, fixed_t actual,
                        real_t expected) noexcept -> void
        {
            this->error_accumulator = error_accumulator;
            this->arg               = arg;
            this->actual            = actual;
            this->expected          = expected;
        }
    };

    using sut_t = error_metric_t<arg_t, real_t, policy_t, error_accumulator_t>;

    static constexpr auto error_accumulator_id = 5;
    error_accumulator_t   error_accumulator{.id = 5};
    arg_t                 arg{7};
    fixed_t               actual{11};
    real_t                expected{13};
    sut_t                 sut{.policy = {}, .error_accumulator = error_accumulator};
};

TEST_F(error_metric_test_t, call_operator)
{
    sut.sample(arg, actual, expected);

    EXPECT_EQ(error_accumulator, sut.policy.error_accumulator);
    EXPECT_EQ(arg, sut.policy.arg);
    EXPECT_EQ(actual, sut.policy.actual);
    EXPECT_EQ(expected, sut.policy.expected);
}

TEST_F(error_metric_test_t, ostream_inserter)
{
    auto expected = std::ostringstream{};
    expected << "error_accumulator " << error_accumulator_id;

    auto actual = std::ostringstream{};
    actual << sut;

    EXPECT_EQ(expected.str(), actual.str());
}

// ====================================================================================================================
// Error Metrics
// ====================================================================================================================

struct error_metrics_test_t : Test
{
    using arg_t   = int_t;
    using fixed_t = int_t;
    using real_t  = int_t;

    struct sample_results_t
    {
        arg_t   arg{};
        fixed_t actual{};
        real_t  expected{};

        constexpr auto operator==(sample_results_t const&) const noexcept -> bool = default;
    };

    struct metric_t
    {
        std::string_view name;

        sample_results_t sample_results{};

        constexpr auto sample(arg_t arg, fixed_t actual, real_t expected) noexcept -> void
        {
            sample_results.arg      = arg;
            sample_results.actual   = actual;
            sample_results.expected = expected;
        }

        friend auto operator<<(std::ostream& out, metric_t const& src) -> std::ostream& { return out << src.name; }
    };

    struct policy_t
    {
        using arg_t  = int_t;
        using real_t = int_t;

        template <typename, typename> using diff_metric_t = metric_t;
        template <typename, typename> using rel_metric_t  = metric_t;
        template <typename, typename> using ulps_metric_t = metric_t;
    };

    using sut_t = error_metrics_t<policy_t>;

    static constexpr auto expected_sample_results = sample_results_t{
        .arg      = 7,
        .actual   = 11,
        .expected = 13,
    };

    sut_t sut{
        .diff_metric = metric_t{.name = "diff"},
        .rel_metric  = metric_t{.name = "rel"},
        .ulps_metric = metric_t{.name = "ulps"},
    };
};

TEST_F(error_metrics_test_t, sample)
{
    sut.sample(expected_sample_results.arg, expected_sample_results.actual, expected_sample_results.expected);

    EXPECT_EQ(expected_sample_results, sut.diff_metric.sample_results);
    EXPECT_EQ(expected_sample_results, sut.rel_metric.sample_results);
    EXPECT_EQ(expected_sample_results, sut.ulps_metric.sample_results);
}

TEST_F(error_metrics_test_t, ostream_inserter)
{
    auto const expected = "diff:\ndiff\nrel:\nrel\nulps:\nulps";

    auto actual = std::ostringstream{};
    actual << sut;

    EXPECT_EQ(expected, actual.str());
}

// ====================================================================================================================
// Accuracy Metrics
// ====================================================================================================================

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
