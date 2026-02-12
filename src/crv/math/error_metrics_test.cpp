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
// Individual Error Metrics
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// Signed Diff
// --------------------------------------------------------------------------------------------------------------------

struct error_metric_diff_test_t : Test
{
    using arg_t   = int_t;
    using value_t = float_t;

    arg_t const   arg{3};
    value_t const actual{7};
    value_t const error{13};

    StrictMock<mock_sampler_t> mock_error_accumulator;

    using error_accumulator_t = sampler_t;

    using sut_t = error_metric::diff_t<arg_t, value_t, error_accumulator_t>;
    sut_t sut{error_accumulator_t{"error_accumulator", &mock_error_accumulator}};

    auto test_sample(value_t error) noexcept -> void
    {
        EXPECT_CALL(mock_error_accumulator, sample(arg, error));
        sut.sample(arg, actual + error, actual);
    }
};

TEST_F(error_metric_diff_test_t, sample_zero_error)
{
    test_sample(0);
}

TEST_F(error_metric_diff_test_t, sample_positive_error)
{
    test_sample(error);
}

TEST_F(error_metric_diff_test_t, sample_negative_error)
{
    test_sample(-error);
}

TEST_F(error_metric_diff_test_t, ostream_inserter)
{
    auto const expected = "error_accumulator";

    auto actual = std::ostringstream{};
    EXPECT_EQ(&actual, &(actual << sut));

    EXPECT_EQ(expected, actual.str());
}

// --------------------------------------------------------------------------------------------------------------------
// Signed Rel Diff
// --------------------------------------------------------------------------------------------------------------------

struct error_metric_rel_test_t : Test
{
    using arg_t   = int_t;
    using value_t = float_t;

    arg_t const   arg{3};
    value_t const actual{7};
    value_t const error{13};

    StrictMock<mock_sampler_t> mock_error_accumulator;

    using error_accumulator_t = sampler_t;

    using sut_t = error_metric::rel_t<arg_t, value_t, error_accumulator_t>;
    sut_t sut{error_accumulator_t{"error_accumulator", &mock_error_accumulator}};

    auto test_sample(value_t error) noexcept -> void
    {
        EXPECT_CALL(mock_error_accumulator, sample(arg, error / actual));
        sut.sample(arg, actual + error, actual);
    }
};

TEST_F(error_metric_rel_test_t, sample_expected_zero_is_ignored)
{
    sut.sample(arg, actual, 0.0);
}

TEST_F(error_metric_rel_test_t, sample_positive_error)
{
    test_sample(error);
}

TEST_F(error_metric_rel_test_t, sample_negative_error)
{
    test_sample(-error);
}

TEST_F(error_metric_rel_test_t, ostream_inserter)
{
    auto const expected = "error_accumulator";

    auto actual = std::ostringstream{};
    EXPECT_EQ(&actual, &(actual << sut));

    EXPECT_EQ(expected, actual.str());
}

// --------------------------------------------------------------------------------------------------------------------
// Signed Ulps
// --------------------------------------------------------------------------------------------------------------------

struct error_metric_ulps_test_t : Test
{
    using arg_t   = int_t;
    using value_t = float_t;
    using ulps_t  = int_t;
    using fixed_t = fixed_t<int_t, 0>;

    arg_t const   arg{3};
    fixed_t const actual_fixed{7};
    value_t const actual_float{from_fixed<value_t>(actual_fixed)};
    ulps_t const  error{13};

    StrictMock<mock_sampler_t> mock_error_accumulator;
    StrictMock<mock_sampler_t> mock_distribution;
    StrictMock<mock_sampler_t> mock_fr_frac;

    using error_accumulator_t = sampler_t;
    using distribution_t      = sampler_t;
    using fr_frac_t           = sampler_t;

    using sut_t = error_metric::ulps_t<arg_t, value_t, fixed_t, error_accumulator_t, distribution_t, fr_frac_t>;
    sut_t sut{error_accumulator_t{"error_accumulator", &mock_error_accumulator},
              distribution_t{"distribution", &mock_distribution}, fr_frac_t{"fr_frac", &mock_fr_frac}};

    auto test_sample(ulps_t error) noexcept -> void
    {
        EXPECT_CALL(mock_error_accumulator, sample(arg, error));
        EXPECT_CALL(mock_distribution, sample(error));
        EXPECT_CALL(mock_fr_frac, sample(error));
        sut.sample(arg, fixed_t{actual_fixed.value + error}, from_fixed<value_t>(actual_fixed));
    }
};

TEST_F(error_metric_ulps_test_t, sample_zero_error)
{
    test_sample(0);
}

TEST_F(error_metric_ulps_test_t, one_higher)
{
    test_sample(error);
}

TEST_F(error_metric_ulps_test_t, ten_lower)
{
    test_sample(-10 * error);
}

TEST_F(error_metric_ulps_test_t, ostream_inserter)
{
    auto const expected = "error_accumulator\ndistribution\nfr_frac = fr_frac";

    auto actual = std::ostringstream{};
    EXPECT_EQ(&actual, &(actual << sut));

    EXPECT_EQ(expected, actual.str());
}

// --------------------------------------------------------------------------------------------------------------------
// Montonicity
// --------------------------------------------------------------------------------------------------------------------

struct error_metric_mono_test_t : Test
{
    using arg_t   = int_t;
    using value_t = float_t;
    using fixed_t = fixed_t<int_t, 0>;

    arg_t const   arg{3};
    fixed_t const actual_fixed{7};
    value_t const actual_float{from_fixed<value_t>(actual_fixed)};
    fixed_t const error{13};

    StrictMock<mock_sampler_t> mock_error_accumulator;

    using error_accumulator_t = sampler_t;
    struct counting_error_accumulator_t : sampler_t
    {
        int_t sample_count{0};
    };

    using distribution_t = sampler_t;
    using fr_frac_t      = sampler_t;

    using sut_t = error_metric::mono_t<arg_t, value_t, fixed_t, counting_error_accumulator_t>;
    sut_t sut{counting_error_accumulator_t{error_accumulator_t{"error_accumulator", &mock_error_accumulator}}};
};

TEST_F(error_metric_mono_test_t, sample_no_prev)
{
    auto const expected = actual_fixed;

    sut.sample(arg, expected);

    EXPECT_EQ(expected, *sut.prev);
    EXPECT_EQ(0, sut.violation_count);
}

TEST_F(error_metric_mono_test_t, sample_zero_error)
{
    sut.prev = actual_fixed;
    EXPECT_CALL(mock_error_accumulator, sample(arg, 0.0));

    sut.sample(arg, actual_fixed);

    EXPECT_EQ(actual_fixed, *sut.prev);
    EXPECT_EQ(0, sut.violation_count);
}

TEST_F(error_metric_mono_test_t, sample_positive_error)
{
    sut.prev = actual_fixed;
    EXPECT_CALL(mock_error_accumulator, sample(arg, from_fixed<value_t>(error)));

    sut.sample(arg, actual_fixed + error);

    EXPECT_EQ(actual_fixed + error, *sut.prev);
    EXPECT_EQ(0, sut.violation_count);
}

TEST_F(error_metric_mono_test_t, sample_negative_error)
{
    sut.prev = actual_fixed;
    EXPECT_CALL(mock_error_accumulator, sample(arg, 0.0));

    auto const actual   = actual_fixed;
    auto const expected = actual - error;
    sut.sample(arg, expected);

    EXPECT_EQ(actual_fixed - error, *sut.prev);
    EXPECT_EQ(1, sut.violation_count);
}

TEST_F(error_metric_mono_test_t, ostream_inserter_zero_sample_count)
{
    auto const expected = "violations = 0 (0%)";

    auto actual = std::ostringstream{};
    EXPECT_EQ(&actual, &(actual << sut));

    EXPECT_EQ(expected, actual.str());
}

TEST_F(error_metric_mono_test_t, ostream_inserter_nonzero_sample_count)
{
    sut.violation_count                = 73;
    sut.error_accumulator.sample_count = sut.violation_count * 2;
    auto const expected                = "violations = 73 (50%)\nerror_accumulator";

    auto actual = std::ostringstream{};
    EXPECT_EQ(&actual, &(actual << sut));

    EXPECT_EQ(expected, actual.str());
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
    using fixed_t = fixed_t<int_t, 0>;
    using value_t = float_t;

    template <typename actual_t> struct sample_results_t
    {
        arg_t    arg{};
        actual_t actual{};
        value_t  expected{};

        constexpr auto operator==(sample_results_t const&) const noexcept -> bool = default;
    };

    template <typename actual_t> struct common_metric_t
    {
        std::string_view name;

        sample_results_t<actual_t> sample_results{};

        friend auto operator<<(std::ostream& out, common_metric_t const& src) -> std::ostream&
        {
            return out << src.name;
        }
    };

    template <typename actual_t> struct metric_t : common_metric_t<actual_t>
    {
        constexpr auto sample(arg_t arg, actual_t actual, value_t expected) noexcept -> void
        {
            this->sample_results.arg      = arg;
            this->sample_results.actual   = actual;
            this->sample_results.expected = expected;
        }
    };

    // mono metric has a differerent sample() shape
    struct mono_metric_t : common_metric_t<fixed_t>
    {
        constexpr auto sample(arg_t arg, fixed_t actual) noexcept -> void
        {
            sample_results.arg    = arg;
            sample_results.actual = actual;
        }
    };

    struct policy_t
    {
        using arg_t   = arg_t;
        using value_t = value_t;
        using fixed_t = fixed_t;

        template <typename, typename> using diff_metric_t           = metric_t<value_t>;
        template <typename, typename> using rel_metric_t            = metric_t<value_t>;
        template <typename, typename, typename> using ulps_metric_t = metric_t<fixed_t>;
        template <typename, typename, typename> using mono_metric_t = mono_metric_t;
    };

    using sut_t = error_metrics_t<policy_t>;

    static constexpr auto expected_metric_sample_results = sample_results_t<value_t>{
        .arg      = 7,
        .actual   = 11,
        .expected = 13,
    };

    static constexpr auto expected_accumulator_sample_results = sample_results_t<fixed_t>{
        .arg      = 7,
        .actual   = 11,
        .expected = 0,
    };

    sut_t sut{
        .diff_metric = metric_t<value_t>{{.name = "diff"}},
        .rel_metric  = metric_t<value_t>{{.name = "rel"}},
        .ulps_metric = metric_t<fixed_t>{{.name = "ulps"}},
        .mono_metric = mono_metric_t{{.name = "mono"}},
    };
};

TEST_F(error_metrics_test_t, sample)
{
    sut.sample(expected_metric_sample_results.arg, expected_accumulator_sample_results.actual,
               expected_metric_sample_results.expected);

    EXPECT_EQ(expected_metric_sample_results.arg, sut.diff_metric.sample_results.arg);
    EXPECT_EQ(expected_accumulator_sample_results.actual, sut.diff_metric.sample_results.actual);
    EXPECT_EQ(expected_metric_sample_results.expected, sut.diff_metric.sample_results.expected);

    EXPECT_EQ(expected_metric_sample_results, sut.rel_metric.sample_results);

    EXPECT_EQ(expected_metric_sample_results.arg, sut.ulps_metric.sample_results.arg);
    EXPECT_EQ(expected_accumulator_sample_results.actual, sut.ulps_metric.sample_results.actual);
    EXPECT_EQ(expected_metric_sample_results.expected, sut.ulps_metric.sample_results.expected);

    EXPECT_EQ(expected_accumulator_sample_results, sut.mono_metric.sample_results);
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
