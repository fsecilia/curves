// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include "error_metrics.hpp"
#include <crv/math/fixed/conversions.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/test/test.hpp>
#include <algorithm>
#include <map>
#include <random>
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
    using sut_t   = arg_max_t<arg_t, value_t>;

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
    using sut_t   = arg_min_t<arg_t, value_t>;

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
    static constexpr auto arg   = arg_t{19};
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
// Histograms
// ====================================================================================================================

struct integer_histogram_test_t : Test
{
    using value_t  = int_t;
    using sut_t    = histogram_t<value_t>;
    using values_t = sut_t::values_t;

    using dump_t = std::map<value_t, int_t>;
    auto dump(sut_t const& sut) const -> dump_t
    {
        auto result = dump_t{};
        sut.visit([&](auto value, auto count) {
            result[value] = count;
            return true;
        });
        return result;
    }
};

TEST_F(integer_histogram_test_t, strips_trailing_zeros)
{
    sut_t const expected({0, 1}, {0, 0, 1});

    sut_t const actual({0, 1, 0}, {0, 0, 1, 0, 0});

    EXPECT_EQ(expected, actual);
}

// --------------------------------------------------------------------------------------------------------------------
// Default Constructed
// --------------------------------------------------------------------------------------------------------------------

struct integer_histogram_test_default_constructed_t : integer_histogram_test_t
{
    sut_t sut{};
};

TEST_F(integer_histogram_test_default_constructed_t, dump)
{
    EXPECT_TRUE(dump(sut).empty());
}

TEST_F(integer_histogram_test_default_constructed_t, initially_empty)
{
    EXPECT_TRUE(!sut.count());
}

TEST_F(integer_histogram_test_default_constructed_t, sample_zero)
{
    sut.sample(0);
    EXPECT_EQ((dump_t{{0, 1}}), dump(sut));
    EXPECT_EQ(1, sut.count());
}

TEST_F(integer_histogram_test_default_constructed_t, single_negative)
{
    sut.sample(-3);
    EXPECT_EQ((dump_t{{-3, 1}}), dump(sut));
    EXPECT_EQ(1, sut.count());
}

TEST_F(integer_histogram_test_default_constructed_t, single_positive)
{
    sut.sample(3);
    EXPECT_EQ((dump_t{{3, 1}}), dump(sut));
    EXPECT_EQ(1, sut.count());
}

TEST_F(integer_histogram_test_default_constructed_t, expands_negative)
{
    sut.sample(-3);
    sut.sample(-5);
    EXPECT_EQ((dump_t{{-5, 1}, {-3, 1}}), dump(sut));
    EXPECT_EQ(2, sut.count());
}

TEST_F(integer_histogram_test_default_constructed_t, expands_positive)
{
    sut.sample(3);
    sut.sample(5);
    EXPECT_EQ((dump_t{{3, 1}, {5, 1}}), dump(sut));
    EXPECT_EQ(2, sut.count());
}

TEST_F(integer_histogram_test_default_constructed_t, does_not_contract_negative)
{
    sut.sample(-5);
    sut.sample(-3);
    EXPECT_EQ((dump_t{{-5, 1}, {-3, 1}}), dump(sut));
    EXPECT_EQ(2, sut.count());
}

TEST_F(integer_histogram_test_default_constructed_t, does_not_contract_positive)
{
    sut.sample(5);
    sut.sample(3);
    EXPECT_EQ((dump_t{{3, 1}, {5, 1}}), dump(sut));
    EXPECT_EQ(2, sut.count());
}

TEST_F(integer_histogram_test_default_constructed_t, multiple_samples_sum)
{
    sut.sample(5);
    sut.sample(-3);
    sut.sample(5);
    EXPECT_EQ((dump_t{{-3, 1}, {5, 2}}), dump(sut));
    EXPECT_EQ(3, sut.count());
}

TEST_F(integer_histogram_test_default_constructed_t, equality)
{
    sut.sample(5);
    sut.sample(-3);
    sut.sample(5);
    EXPECT_EQ((sut_t{{0, 0, 0, 1}, {0, 0, 0, 0, 0, 2}}), sut);
}

TEST_F(integer_histogram_test_default_constructed_t, ostream_inserter)
{
    auto const expected = "{}";

    auto actual = std::ostringstream{};
    actual << sut;

    EXPECT_EQ(expected, actual.str());
}

// --------------------------------------------------------------------------------------------------------------------
// Nontrivially Constructed
// --------------------------------------------------------------------------------------------------------------------

struct integer_histogram_test_nontrivially_constructed_t : integer_histogram_test_t
{
    sut_t sut{{0, 3, 7, 0, 13}, {2, 5, 0, 11, 17, 19}};
};

TEST_F(integer_histogram_test_nontrivially_constructed_t, dump)
{
    auto const expected = dump_t{{-4, 13}, {-2, 7}, {-1, 3}, {0, 2}, {1, 5}, {3, 11}, {4, 17}, {5, 19}};

    auto const actual = dump(sut);

    EXPECT_EQ(expected, actual);
    EXPECT_EQ(77, sut.count());
}

TEST_F(integer_histogram_test_nontrivially_constructed_t, copy)
{
    auto const copy = sut;
    EXPECT_EQ(sut, copy);
}

TEST_F(integer_histogram_test_nontrivially_constructed_t, modified_copy)
{
    auto modified_copy = sut;
    modified_copy.sample(1000);
    EXPECT_NE(sut, modified_copy);
    EXPECT_EQ(sut.count() + 1, modified_copy.count());
}

TEST_F(integer_histogram_test_nontrivially_constructed_t, ostream_inserter)
{
    auto const expected = "{{-4, 13}, {-2, 7}, {-1, 3}, {0, 2}, {1, 5}, {3, 11}, {4, 17}, {5, 19}}";

    auto actual = std::ostringstream{};
    actual << sut;

    EXPECT_EQ(expected, actual.str());
}

// ====================================================================================================================
// Histogram Percentiles
// ====================================================================================================================

struct percentiles_calculator_test_t : Test
{
    using data_t = std::vector<int_t>;
    struct oracle_t
    {
        auto operator()(data_t data) const noexcept -> percentile_calculator_t::result_t
        {
            if (data.empty()) return {};

            std::sort(data.begin(), data.end());
            auto const total = static_cast<int64_t>(data.size());

            auto const percentile = [&](auto percentage) noexcept {
                auto const target_count = (total * percentage + 99) / 100;
                return data[target_count - 1];
            };
            return {percentile(50), percentile(90), percentile(95), percentile(99), percentile(100)};
        }
    };

    using histogram_t = histogram_t<int_t>;

    using sut_t    = percentile_calculator_t;
    using result_t = sut_t::result_t;
    sut_t sut{};
};

TEST_F(percentiles_calculator_test_t, empty)
{
    auto const expected = result_t{};

    auto const actual = sut(histogram_t{});

    EXPECT_EQ(expected, actual);
}

TEST_F(percentiles_calculator_test_t, segments_1)
{
    auto const expected = result_t{.p50 = 10, .p90 = 10, .p95 = 10, .p99 = 10, .p100 = 10};

    auto histogram = histogram_t{};

    histogram.sample(10);

    auto const actual = sut(histogram);

    EXPECT_EQ(expected, actual);
}

TEST_F(percentiles_calculator_test_t, segments_2)
{
    auto const expected = result_t{.p50 = -10, .p90 = 10, .p95 = 10, .p99 = 10, .p100 = 10};

    auto histogram = histogram_t{};

    histogram.sample(-10);
    histogram.sample(10);

    auto const actual = sut(histogram);

    EXPECT_EQ(expected, actual);
}

TEST_F(percentiles_calculator_test_t, segments_10)
{
    auto const expected = result_t{.p50 = -10, .p90 = 10, .p95 = 100, .p99 = 100, .p100 = 100};

    auto histogram = histogram_t{};

    histogram.sample(-10);
    histogram.sample(-10);
    histogram.sample(-10);
    histogram.sample(-10);
    histogram.sample(-10);

    histogram.sample(10);
    histogram.sample(10);
    histogram.sample(10);
    histogram.sample(10);
    histogram.sample(100);

    auto const actual = sut(histogram);

    EXPECT_EQ(expected, actual);
}

TEST_F(percentiles_calculator_test_t, segments_20)
{
    auto const expected = result_t{.p50 = -10, .p90 = 10, .p95 = 50, .p99 = 100, .p100 = 100};

    auto histogram = histogram_t{};

    histogram.sample(-10);
    histogram.sample(-10);
    histogram.sample(-10);
    histogram.sample(-10);
    histogram.sample(-10);

    histogram.sample(-10);
    histogram.sample(-10);
    histogram.sample(-10);
    histogram.sample(-10);
    histogram.sample(-10);

    histogram.sample(10);
    histogram.sample(10);
    histogram.sample(10);
    histogram.sample(10);
    histogram.sample(10);

    histogram.sample(10);
    histogram.sample(10);
    histogram.sample(10);
    histogram.sample(50);
    histogram.sample(100);

    auto const actual = sut(histogram);

    EXPECT_EQ(expected, actual);
}

TEST_F(percentiles_calculator_test_t, segments_100)
{
    auto const expected = result_t{.p50 = 50, .p90 = 90, .p95 = 95, .p99 = 99, .p100 = 100};

    auto histogram = histogram_t{};
    for (auto value = 1; value <= 100; ++value) histogram.sample(value);

    auto const actual = sut(histogram);

    EXPECT_EQ(expected, actual);
}

TEST_F(percentiles_calculator_test_t, all_in_one_bin)
{
    auto const expected = result_t{.p50 = 5, .p90 = 5, .p95 = 5, .p99 = 5, .p100 = 5};

    auto histogram = histogram_t{};
    histogram.sample(5);
    histogram.sample(5);
    histogram.sample(5);
    histogram.sample(5);
    histogram.sample(5);

    histogram.sample(5);
    histogram.sample(5);
    histogram.sample(5);
    histogram.sample(5);
    histogram.sample(5);

    auto const actual = sut(histogram);

    EXPECT_EQ(expected, actual);
}

TEST_F(percentiles_calculator_test_t, sparse_step)
{
    auto const expected = result_t{.p50 = 0, .p90 = 1000, .p95 = 1000, .p99 = 1000, .p100 = 1000};

    auto histogram = histogram_t{};
    histogram.sample(-1000);
    histogram.sample(0);
    histogram.sample(1000);

    auto const actual = sut(histogram);

    EXPECT_EQ(expected, actual);
}

// --------------------------------------------------------------------------------------------------------------------
// Fuzz Testing
// --------------------------------------------------------------------------------------------------------------------

struct percentiles_calculator_fuzz_test_t : percentiles_calculator_test_t
{
    struct oracle_t
    {
        auto operator()(data_t data) const noexcept -> percentile_calculator_t::result_t
        {
            if (data.empty()) return {};

            std::sort(data.begin(), data.end());
            auto const total = static_cast<int64_t>(data.size());

            auto const percentile = [&](auto percentage) noexcept {
                auto const target_count = (total * percentage + 99) / 100;
                return data[target_count - 1];
            };
            return {percentile(50), percentile(90), percentile(95), percentile(99), percentile(100)};
        }
    };
    oracle_t oracle{};

    int_t const iteration_count = 1000;

    std::mt19937 rng{0xF012345678};

    std::uniform_int_distribution<int_t> value_distribution{-1000, 1000};
    std::uniform_int_distribution<int_t> size_distribution{1, 10000};

    auto fuzz(data_t const& data, histogram_t const& histogram, int_t iteration) const -> void
    {
        auto const expected = oracle(data);

        auto const actual = sut(histogram);

        EXPECT_EQ(expected, actual) << "mismatch on iteration " << iteration << "!\n"
                                    << "samples: " << std::size(data) << "\n"
                                    << "expected: " << expected << "\n"
                                    << "actual:   " << actual << "\n";
    }

    auto fuzz(int_t iteration) -> void
    {
        auto const size = size_distribution(rng);

        auto data = data_t{};
        data.reserve(size);

        auto histogram = histogram_t{};

        for (auto sample_index = 0; sample_index < size; ++sample_index)
        {
            auto const value = value_distribution(rng);
            data.push_back(value);
            histogram.sample(value);
        }

        fuzz(data, histogram, iteration);
    }

    auto fuzz() -> void
    {
        for (auto iteration = 0; iteration < iteration_count; ++iteration) { fuzz(iteration); }
    }
};

TEST_F(percentiles_calculator_fuzz_test_t, fuzz)
{
    fuzz();
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
        using arg_t   = arg_t;
        using value_t = value_t;

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
