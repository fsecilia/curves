// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include "error_metrics.hpp"
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
    using arg_t   = int_t;
    using value_t = float_t;
    using sut_t   = arg_max_t<int_t, value_t>;

    static constexpr auto old_max = 3.0;
    static constexpr auto old_arg = 5;
    static constexpr auto new_arg = 10;

    sut_t sut{.value = old_max, .arg = old_arg};
};

TEST_F(arg_max_test_t, initializes_to_min)
{
    ASSERT_EQ(min<value_t>(), sut_t{}.value);
}

TEST_F(arg_max_test_t, sample_without_new_max)
{
    auto const value = old_max - 1;
    sut.sample(new_arg, value);

    ASSERT_EQ(old_max, sut.value);
    ASSERT_EQ(old_arg, sut.arg);
}

TEST_F(arg_max_test_t, first_wins)
{
    auto const value = old_max;
    sut.sample(new_arg, value);

    ASSERT_EQ(old_max, sut.value);
    ASSERT_EQ(old_arg, sut.arg);
}

TEST_F(arg_max_test_t, sample_new_max)
{
    auto const new_max = old_max + 1;
    sut.sample(new_arg, new_max);

    ASSERT_EQ(new_max, sut.value);
    ASSERT_EQ(new_arg, sut.arg);
}

TEST_F(arg_max_test_t, ostream_inserter)
{
    auto expected = std::ostringstream{};
    expected << old_max << "@" << old_arg;

    auto actual = std::ostringstream{};
    actual << sut;

    ASSERT_EQ(expected.str(), actual.str());
}

// ====================================================================================================================
// Arg Min
// ====================================================================================================================

struct arg_min_test_t : Test
{
    using arg_t   = int_t;
    using value_t = float_t;
    using sut_t   = arg_min_t<int_t, value_t>;

    static constexpr auto old_min = 3.0;
    static constexpr auto old_arg = 5;
    static constexpr auto new_arg = 10;

    sut_t sut{.value = old_min, .arg = old_arg};
};

TEST_F(arg_min_test_t, initializes_to_max)
{
    ASSERT_EQ(max<value_t>(), sut_t{}.value);
}

TEST_F(arg_min_test_t, sample_without_new_min)
{
    auto const value = old_min + 1;
    sut.sample(new_arg, value);

    ASSERT_EQ(old_min, sut.value);
    ASSERT_EQ(old_arg, sut.arg);
}

TEST_F(arg_min_test_t, first_wins)
{
    auto const value = old_min;
    sut.sample(new_arg, value);

    ASSERT_EQ(old_min, sut.value);
    ASSERT_EQ(old_arg, sut.arg);
}

TEST_F(arg_min_test_t, sample_new_min)
{
    auto const new_min = old_min - 1;
    sut.sample(new_arg, new_min);

    ASSERT_EQ(new_min, sut.value);
    ASSERT_EQ(new_arg, sut.arg);
}

TEST_F(arg_min_test_t, ostream_inserter)
{
    auto expected = std::ostringstream{};
    expected << old_min << "@" << old_arg;

    auto actual = std::ostringstream{};
    actual << sut;

    ASSERT_EQ(expected.str(), actual.str());
}

// ====================================================================================================================
// Min Max
// ====================================================================================================================

struct min_max_test_t : Test
{
    using arg_t   = int_t;
    using value_t = float_t;

    static constexpr arg_t   arg_min     = 3;
    static constexpr value_t min         = 1.1;
    static constexpr arg_t   arg_max     = 5;
    static constexpr value_t max         = 1.2;
    static constexpr arg_t   arg_max_mag = arg_max;
    static constexpr value_t max_mag     = 1.2;

    struct arg_min_t
    {
        arg_t   arg{arg_min};
        value_t value{min};

        constexpr auto sample(arg_t arg, value_t value) noexcept -> void
        {
            this->arg   = arg;
            this->value = value;
        }

        friend auto operator<<(std::ostream& out, arg_min_t const&) -> std::ostream& { return out << "arg_min"; }
    };

    struct arg_max_t
    {
        arg_t   arg{min_max_test_t::arg_max};
        value_t value{min_max_test_t::max};

        constexpr auto sample(arg_t arg, value_t value) noexcept -> void
        {
            this->arg   = arg;
            this->value = value;
        }

        friend auto operator<<(std::ostream& out, arg_max_t const&) -> std::ostream& { return out << "arg_max"; }
    };

    using sut_t = min_max_t<arg_t, value_t, arg_min_t, arg_max_t>;
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
    static constexpr auto arg   = value_t{19.0};
    static constexpr auto value = value_t{17.0};

    sut.sample(arg, value);

    EXPECT_EQ(arg, sut.max.arg);
    EXPECT_EQ(value, sut.max.value);
    EXPECT_EQ(arg, sut.min.arg);
    EXPECT_EQ(value, sut.min.value);
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
    using value_t       = float_t;
    using accumulator_t = float_t;

    static constexpr auto arg          = arg_t{3};
    static constexpr auto error        = value_t{15.2};
    static constexpr auto sample_count = int_t{11};

    static constexpr auto rmse = value_t{23.6};
    static constexpr auto mse  = rmse * rmse;
    static constexpr auto sse  = mse * sample_count;

    static constexpr auto bias     = 7.3;
    static constexpr auto variance = mse - bias * bias;
    static constexpr auto sum      = bias * sample_count;

    struct min_max_t
    {
        arg_t   arg{};
        value_t error{};

        constexpr auto sample(arg_t arg, value_t error) noexcept -> void
        {
            this->arg   = arg;
            this->error = error;
        }

        friend auto operator<<(std::ostream& out, min_max_t const&) -> std::ostream& { return out << "min_max"; }
    };

    using sut_t = error_accumulator_t<arg_t, value_t, accumulator_t, min_max_t>;
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
    using value_t = float_t;
    using fixed_t = fixed_t<int64_t, 32>;

    static constexpr auto arg      = arg_t{372};
    static constexpr auto error    = value_t{1.0};
    static constexpr auto expected = value_t{4.0};

    struct error_accumulator_t
    {
        using arg_t   = arg_t;
        using value_t = value_t;

        arg_t   arg{};
        value_t error{};

        auto sample(arg_t arg, value_t error) noexcept -> void
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

    sut(error_accumulator, arg, fixed_t{1}, 0.0);

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
    using value_t = float_t;

    struct policy_t
    {
        error_accumulator_t error_accumulator{};
        arg_t               arg{};
        fixed_t             actual{};
        value_t             expected{};
        auto                operator()(error_accumulator_t const& error_accumulator, arg_t arg, fixed_t actual,
                        value_t expected) noexcept -> void
        {
            this->error_accumulator = error_accumulator;
            this->arg               = arg;
            this->actual            = actual;
            this->expected          = expected;
        }
    };

    using sut_t = error_metric_t<arg_t, value_t, policy_t, error_accumulator_t>;

    static constexpr auto error_accumulator_id = 5;
    error_accumulator_t   error_accumulator{.id = 5};
    arg_t                 arg{7};
    fixed_t               actual{11};
    value_t               expected{13};
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
    using value_t = float_t;

    struct sample_results_t
    {
        arg_t   arg{};
        fixed_t actual{};
        value_t expected{};

        constexpr auto operator==(sample_results_t const&) const noexcept -> bool = default;
    };

    struct metric_t
    {
        std::string_view name;

        sample_results_t sample_results{};

        constexpr auto sample(arg_t arg, fixed_t actual, value_t expected) noexcept -> void
        {
            sample_results.arg      = arg;
            sample_results.actual   = actual;
            sample_results.expected = expected;
        }

        friend auto operator<<(std::ostream& out, metric_t const& src) -> std::ostream& { return out << src.name; }
    };

    struct policy_t
    {
        using arg_t   = int_t;
        using value_t = int_t;

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

} // namespace
} // namespace crv
