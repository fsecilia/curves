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
        EXPECT_CALL(mock_error_accumulator, sample(arg, static_cast<value_t>(error)));
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
// Montonicity Direction Policies
// --------------------------------------------------------------------------------------------------------------------

struct error_metric_mono_dir_policies_test_t : Test
{
    using fixed_t = fixed_t<int_t, 0>;

    fixed_t lesser       = fixed_t{3};
    fixed_t greater      = fixed_t{5};
    fixed_t violation    = fixed_t{2};
    fixed_t no_violation = fixed_t{0};
};

TEST_F(error_metric_mono_dir_policies_test_t, ascending_no_violation)
{
    EXPECT_EQ(no_violation, error_metric::mono_dir_policies::ascending_t{}(lesser, greater));
}

TEST_F(error_metric_mono_dir_policies_test_t, ascending_violation)
{
    EXPECT_EQ(violation, error_metric::mono_dir_policies::ascending_t{}(greater, lesser));
}

TEST_F(error_metric_mono_dir_policies_test_t, descending_no_violation)
{
    EXPECT_EQ(no_violation, error_metric::mono_dir_policies::descending_t{}(greater, lesser));
}

TEST_F(error_metric_mono_dir_policies_test_t, descending_violation)
{
    EXPECT_EQ(violation, error_metric::mono_dir_policies::descending_t{}(lesser, greater));
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
    fixed_t const prev{7};
    fixed_t const cur{13};

    struct mock_dir_policy_t
    {
        MOCK_METHOD(fixed_t, call, (fixed_t prev, fixed_t cur), (const, noexcept));
        virtual ~mock_dir_policy_t() = default;
    };
    StrictMock<mock_dir_policy_t> mock_dir_policy;

    struct dir_policy_t
    {
        mock_dir_policy_t* mock = nullptr;

        auto operator()(fixed_t prev, fixed_t cur) const noexcept -> fixed_t { return mock->call(prev, cur); }
    };

    StrictMock<mock_sampler_t> mock_error_accumulator;

    using error_accumulator_t = sampler_t;
    struct counting_error_accumulator_t : sampler_t
    {
        int_t sample_count{0};
    };

    using distribution_t = sampler_t;
    using fr_frac_t      = sampler_t;

    using sut_t = error_metric::mono_t<arg_t, value_t, fixed_t, dir_policy_t, counting_error_accumulator_t>;
    sut_t sut{dir_policy_t{&mock_dir_policy},
              counting_error_accumulator_t{error_accumulator_t{"error_accumulator", &mock_error_accumulator}}};
};

TEST_F(error_metric_mono_test_t, sample_no_prev)
{
    sut.sample(arg, cur);

    EXPECT_EQ(cur, *sut.prev);
    EXPECT_EQ(0, sut.violation_count);
}

TEST_F(error_metric_mono_test_t, sample_with_prev_no_violation)
{
    sut.prev = prev;
    EXPECT_CALL(mock_dir_policy, call(prev, cur)).WillOnce(Return(0));
    EXPECT_CALL(mock_error_accumulator, sample(arg, 0.0));

    sut.sample(arg, cur);

    EXPECT_EQ(cur, *sut.prev);
    EXPECT_EQ(0, sut.violation_count);
}

TEST_F(error_metric_mono_test_t, sample_with_prev_violation)
{
    sut.prev                      = prev;
    auto const expected_violation = fixed_t{257};
    EXPECT_CALL(mock_dir_policy, call(prev, cur)).WillOnce(Return(expected_violation));
    EXPECT_CALL(mock_error_accumulator, sample(arg, from_fixed<value_t>(expected_violation)));

    sut.sample(arg, cur);

    EXPECT_EQ(cur, *sut.prev);
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
    actual << sut;

    EXPECT_EQ(expected, actual.str());
}

// ====================================================================================================================
// Error Metrics
// ====================================================================================================================

struct error_metrics_test_t : Test
{
    using arg_t   = int_t;
    using fixed_t = fixed_t<int_t, 0>;
    using value_t = float_t;

    template <typename actual_t> struct metric_t
    {
        std::string_view name;

        arg_t    arg{};
        actual_t actual{};
        value_t  expected{};

        constexpr auto sample(arg_t arg, actual_t actual, value_t expected) noexcept -> void
        {
            this->arg      = arg;
            this->actual   = actual;
            this->expected = expected;
        }

        friend auto operator<<(std::ostream& out, metric_t const& src) -> std::ostream& { return out << src.name; }
    };

    struct mono_metric_t
    {
        std::string_view name;

        arg_t   arg{};
        fixed_t actual{};

        constexpr auto sample(arg_t arg, fixed_t actual) noexcept -> void
        {
            this->arg    = arg;
            this->actual = actual;
        }

        friend auto operator<<(std::ostream& out, mono_metric_t const& src) -> std::ostream& { return out << src.name; }
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

    arg_t const   arg{7};
    value_t const actual_value{11};
    fixed_t const actual_fixed{to_fixed<fixed_t>(actual_value)};
    value_t const expected{13};

    sut_t sut{
        .diff_metric = metric_t<value_t>{.name = "diff"},
        .rel_metric  = metric_t<value_t>{.name = "rel"},
        .ulps_metric = metric_t<fixed_t>{.name = "ulps"},
        .mono_metric = mono_metric_t{.name = "mono"},
    };
};

TEST_F(error_metrics_test_t, sample)
{
    sut.sample(arg, actual_fixed, expected);

    // diff metric
    EXPECT_EQ(arg, sut.diff_metric.arg);
    EXPECT_EQ(actual_value, sut.diff_metric.actual);
    EXPECT_EQ(expected, sut.diff_metric.expected);

    // rel metric
    EXPECT_EQ(arg, sut.rel_metric.arg);
    EXPECT_EQ(actual_value, sut.rel_metric.actual);
    EXPECT_EQ(expected, sut.rel_metric.expected);

    // ulps metric
    EXPECT_EQ(arg, sut.ulps_metric.arg);
    EXPECT_EQ(actual_fixed, sut.ulps_metric.actual);
    EXPECT_EQ(expected, sut.ulps_metric.expected);

    // mono metric
    EXPECT_EQ(arg, sut.mono_metric.arg);
    EXPECT_EQ(actual_fixed, sut.mono_metric.actual);
}

TEST_F(error_metrics_test_t, ostream_inserter)
{
    auto const expected = "diff:\ndiff\nrel:\nrel\nulps:\nulps\nmono:\nmono";

    auto actual = std::ostringstream{};
    EXPECT_EQ(&actual, &(actual << sut));

    EXPECT_EQ(expected, actual.str());
}

} // namespace
} // namespace crv
