// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include "error_metrics.hpp"
#include <crv/math/fixed/conversions.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>
#include <sstream>
#include <string_view>

namespace crv {
namespace {

struct mock_sampler_t
{
    MOCK_METHOD(void, sample, (int_t value));
    MOCK_METHOD(void, sample, (int_t arg, float_t value));
    virtual ~mock_sampler_t() = default;
};

struct sampler_t
{
    std::string_view name;
    mock_sampler_t*  mock = nullptr;

    auto sample(int_t value) -> void { mock->sample(value); }
    auto sample(int_t arg, float_t value) -> void { mock->sample(arg, value); }

    friend auto operator<<(std::ostream& out, sampler_t const& src) -> std::ostream& { return out << src.name; }
};

// ====================================================================================================================
// Faithfully-Rounded Fraction
// ====================================================================================================================

struct fr_frac_test_t : Test
{
    using sut_t = fr_frac_t<float_t>;
    sut_t sut{};
};

TEST_F(fr_frac_test_t, result_initially_zero)
{
    EXPECT_EQ(0, sut.result());
}

TEST_F(fr_frac_test_t, sample_zero)
{
    sut.sample(0);
    EXPECT_EQ(1, sut.faithfully_rounded_count);
    EXPECT_EQ(1, sut.sample_count);
    EXPECT_EQ(1, sut.result());
}

TEST_F(fr_frac_test_t, sample_pass)
{
    sut.sample(1);
    EXPECT_EQ(1, sut.faithfully_rounded_count);
    EXPECT_EQ(1, sut.sample_count);
    EXPECT_EQ(1, sut.result());
}

TEST_F(fr_frac_test_t, sample_fail)
{
    sut.sample(2);
    EXPECT_EQ(0, sut.faithfully_rounded_count);
    EXPECT_EQ(1, sut.sample_count);
    EXPECT_EQ(0, sut.result());
}

TEST_F(fr_frac_test_t, multiple_samples)
{
    sut.sample(0);
    sut.sample(1);
    sut.sample(2);
    sut.sample(0);
    sut.sample(10);

    EXPECT_EQ(3, sut.faithfully_rounded_count);
    EXPECT_EQ(5, sut.sample_count);
    EXPECT_EQ(3.0 / 5.0, sut.result());
}

// ====================================================================================================================
// Distribution
// ====================================================================================================================

struct distribution_test_t : Test
{
    using value_t = int_t;

    StrictMock<mock_sampler_t> mock_histogram;

    using histogram_t = sampler_t;

    using percentile_result_t                        = int_t;
    static constexpr auto expected_percentile_result = percentile_result_t{17};

    struct mock_percentile_calculator_t
    {
        MOCK_METHOD(percentile_result_t, call, (mock_sampler_t const& mock_histogram), (const, noexcept));
        virtual ~mock_percentile_calculator_t() = default;
    };
    StrictMock<mock_percentile_calculator_t> mock_percentilies_calculator;

    struct percentile_calculator_t
    {
        using result_t = percentile_result_t;

        mock_percentile_calculator_t* mock = nullptr;

        auto operator()(histogram_t const& histogram) const noexcept -> result_t { return mock->call(*histogram.mock); }
    };

    using sut_t = distribution_t<histogram_t, percentile_calculator_t>;
    sut_t sut{percentile_calculator_t{&mock_percentilies_calculator}, histogram_t{"histogram", &mock_histogram}};
};

TEST_F(distribution_test_t, calc_percentiles)
{
    EXPECT_CALL(mock_percentilies_calculator, call(Ref(mock_histogram))).WillOnce(Return(expected_percentile_result));

    auto const actual = sut.calc_percentiles();

    EXPECT_EQ(expected_percentile_result, actual);
}

TEST_F(distribution_test_t, sample)
{
    auto const ulps = int_t{13};
    EXPECT_CALL(mock_histogram, sample(ulps));
    sut.sample(ulps);
}

TEST_F(distribution_test_t, ostream_inserter)
{
    EXPECT_CALL(mock_percentilies_calculator, call(Ref(mock_histogram))).WillOnce(Return(expected_percentile_result));

    auto expected = std::ostringstream{};
    expected << expected_percentile_result;

    auto actual = std::ostringstream{};
    actual << sut;

    EXPECT_EQ(expected.str(), actual.str());
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

    struct arg_min_max_t
    {
        arg_t   arg{};
        value_t error{};

        constexpr auto sample(arg_t arg, value_t error) noexcept -> void
        {
            this->arg   = arg;
            this->error = error;
        }

        friend auto operator<<(std::ostream& out, arg_min_max_t const&) -> std::ostream&
        {
            return out << "arg_min_max";
        }
    };

    using sut_t = error_accumulator_t<arg_t, value_t, accumulator_t, arg_min_max_t>;
};

// --------------------------------------------------------------------------------------------------------------------
// Default Constructed
// --------------------------------------------------------------------------------------------------------------------

struct error_accumulator_test_default_constructed_t : error_accumulator_test_t
{
    sut_t sut{};
};

TEST_F(error_accumulator_test_default_constructed_t, mse)
{
    EXPECT_EQ(0, sut.mse());
}

TEST_F(error_accumulator_test_default_constructed_t, bias)
{
    EXPECT_EQ(0, sut.bias());
}

TEST_F(error_accumulator_test_default_constructed_t, ostream_inserter)
{
    auto const expected = "sample count = 0";

    auto actual = std::ostringstream{};
    actual << sut;

    ASSERT_EQ(expected, actual.str());
}

// --------------------------------------------------------------------------------------------------------------------
// Constructed
// --------------------------------------------------------------------------------------------------------------------

struct error_accumulator_test_constructed_t : error_accumulator_test_t
{
    sut_t sut{.sse = sse, .sum = sum, .arg_min_max = {}, .sample_count = sample_count};
};

TEST_F(error_accumulator_test_constructed_t, sample)
{
    sut.sample(arg, error);

    EXPECT_EQ(sample_count + 1, sut.sample_count);
    EXPECT_EQ(sse + error * error, sut.sse);
    EXPECT_EQ(sum + error, sut.sum);
    EXPECT_EQ(arg, sut.arg_min_max.arg);
    EXPECT_EQ(error, sut.arg_min_max.error);
}

TEST_F(error_accumulator_test_constructed_t, mse)
{
    EXPECT_EQ(mse, sut.mse());
}

TEST_F(error_accumulator_test_constructed_t, rmse)
{
    EXPECT_EQ(rmse, sut.rmse());
}

TEST_F(error_accumulator_test_constructed_t, bias)
{
    EXPECT_EQ(bias, sut.bias());
}

TEST_F(error_accumulator_test_constructed_t, variance)
{
    EXPECT_EQ(variance, sut.variance());
}

TEST_F(error_accumulator_test_constructed_t, ostream_inserter)
{
    auto expected = std::ostringstream{};
    expected << "sample count = " << sample_count << "\narg_min_max\nsum = " << sum << "\nmse = " << mse
             << "\nrmse = " << rmse << "\nbias = " << bias << "\nvariance = " << variance;

    auto actual = std::ostringstream{};
    actual << sut;

    ASSERT_EQ(expected.str(), actual.str());
}

// ====================================================================================================================
// Ulps Error Accumulator
// ====================================================================================================================

struct ulps_error_accumulator_test_t : Test
{
    using arg_t = int_t;

    StrictMock<mock_sampler_t> mock_error_accumulator;
    StrictMock<mock_sampler_t> mock_distribution;
    StrictMock<mock_sampler_t> mock_fr_frac;

    using error_accumulator_t = sampler_t;
    using distribution_t      = sampler_t;
    using fr_frac_t           = sampler_t;

    using sut_t = ulps_error_accumulator_t<arg_t, float_t, error_accumulator_t, distribution_t, fr_frac_t>;
    sut_t sut{error_accumulator_t{"error_accumulator", &mock_error_accumulator},
              distribution_t{"distribution", &mock_distribution}, fr_frac_t{"fr_frac", &mock_fr_frac}};
};

TEST_F(ulps_error_accumulator_test_t, sample)
{
    auto const arg   = arg_t{3};
    auto const value = int_t{5};
    EXPECT_CALL(mock_error_accumulator, sample(arg, static_cast<float_t>(value)));
    EXPECT_CALL(mock_distribution, sample(value));
    EXPECT_CALL(mock_fr_frac, sample(value));
    sut.sample(arg, value);
}

TEST_F(ulps_error_accumulator_test_t, ostream_inserter)
{
    auto const expected = "error_accumulator\ndistribution\nfr_frac = fr_frac";

    auto actual = std::ostringstream{};
    EXPECT_EQ(&actual, &(actual << sut));

    EXPECT_EQ(expected, actual.str());
}

// ====================================================================================================================
// Metric Policies
// ====================================================================================================================

template <typename t_value_t = float_t> struct metric_policy_test_t : Test
{
    using arg_t   = int_t;
    using value_t = t_value_t;
    using fixed_t = fixed_t<int64_t, 32>;

    static constexpr auto arg      = arg_t{372};
    static constexpr auto error    = value_t{1};
    static constexpr auto expected = value_t{4};

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

struct metric_policy_test_diff_t : metric_policy_test_t<>
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

struct metric_policy_test_rel_t : metric_policy_test_t<>
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

struct metric_policy_test_ulps_t : metric_policy_test_t<int_t>
{};

TEST_F(metric_policy_test_ulps_t, zero_error)
{
    auto const sut = metric_policy::ulps_t{};

    auto const ideal  = static_cast<int_t>(std::round(std::ldexp(expected, fixed_t::frac_bits)));
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

#if 0
// --------------------------------------------------------------------------------------------------------------------
// Metric Policy Mono
// --------------------------------------------------------------------------------------------------------------------

struct metric_policy_test_mono_t : metric_policy_test_t<>
{
    using sut_t = metric_policy::mono_t<value_t>;
};

TEST_F(metric_policy_test_mono_t, initializes_to_nullopt)
{
    EXPECT_EQ(std::nullopt, sut_t{}.prev);
}

TEST_F(metric_policy_test_mono_t, greater)
{
    auto const prev = 10.0;
    auto       sut  = sut_t{.prev = prev};

    auto const actual = to_fixed<fixed_t>(prev + error);
    sut(error_accumulator, arg, actual, expected);

    EXPECT_EQ(from_fixed<value_t>(actual), sut.prev);
    EXPECT_DOUBLE_EQ(0, error_accumulator.arg);
    EXPECT_DOUBLE_EQ(0, error_accumulator.error);
}

TEST_F(metric_policy_test_mono_t, equal)
{
    auto const prev = 10.0;
    auto       sut  = sut_t{.prev = prev};

    auto const actual = to_fixed<fixed_t>(prev);
    sut(error_accumulator, arg, actual, expected);

    EXPECT_EQ(from_fixed<value_t>(actual), sut.prev);
    EXPECT_DOUBLE_EQ(0, error_accumulator.arg);
    EXPECT_DOUBLE_EQ(0, error_accumulator.error);
}

TEST_F(metric_policy_test_mono_t, lesser)
{
    auto const prev = 10.0;
    auto       sut  = sut_t{.prev = prev};

    auto const actual = to_fixed<fixed_t>(prev - error);
    sut(error_accumulator, arg, actual, expected);

    EXPECT_EQ(from_fixed<value_t>(actual), sut.prev);
    EXPECT_DOUBLE_EQ(arg, error_accumulator.arg);
    EXPECT_DOUBLE_EQ(-error, error_accumulator.error);
}
#endif

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

    struct accumulator_t
    {
        std::string_view name;

        sample_results_t sample_results{};

        constexpr auto sample(arg_t arg, fixed_t actual) noexcept -> void
        {
            sample_results.arg    = arg;
            sample_results.actual = actual;
        }

        friend auto operator<<(std::ostream& out, accumulator_t const& src) -> std::ostream& { return out << src.name; }
    };

    struct policy_t
    {
        using arg_t   = arg_t;
        using value_t = value_t;

        template <typename, typename> using diff_metric_t            = metric_t;
        template <typename, typename> using rel_metric_t             = metric_t;
        template <typename, typename> using ulps_metric_t            = metric_t;
        template <typename, typename> using mono_error_accumulator_t = accumulator_t;
    };

    using sut_t = error_metrics_t<policy_t>;

    static constexpr auto expected_metric_sample_results = sample_results_t{
        .arg      = 7,
        .actual   = 11,
        .expected = 13,
    };

    static constexpr auto expected_accumulator_sample_results = sample_results_t{
        .arg      = 7,
        .actual   = 11,
        .expected = 0,
    };

    sut_t sut{
        .diff_metric            = metric_t{.name = "diff"},
        .rel_metric             = metric_t{.name = "rel"},
        .ulps_metric            = metric_t{.name = "ulps"},
        .mono_error_accumulator = accumulator_t{.name = "mono"},
    };
};

TEST_F(error_metrics_test_t, sample)
{
    sut.sample(expected_metric_sample_results.arg, expected_metric_sample_results.actual,
               expected_metric_sample_results.expected);

    EXPECT_EQ(expected_metric_sample_results, sut.diff_metric.sample_results);
    EXPECT_EQ(expected_metric_sample_results, sut.rel_metric.sample_results);
    EXPECT_EQ(expected_metric_sample_results, sut.ulps_metric.sample_results);
    EXPECT_EQ(expected_accumulator_sample_results, sut.mono_error_accumulator.sample_results);
}

TEST_F(error_metrics_test_t, ostream_inserter)
{
    auto const expected = "diff:\ndiff\nrel:\nrel\nulps:\nulps\nmono:\nmono";

    auto actual = std::ostringstream{};
    actual << sut;

    EXPECT_EQ(expected, actual.str());
}

} // namespace
} // namespace crv
