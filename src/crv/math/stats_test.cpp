// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include "stats.hpp"
#include <crv/test/test.hpp>
#include <algorithm>
#include <gmock/gmock.h>
#include <map>
#include <random>
#include <sstream>

namespace crv {
namespace {

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

struct integer_histogram_test_constructed_t : integer_histogram_test_t
{
    sut_t sut{{0, 3, 7, 0, 13}, {2, 5, 0, 11, 17, 19}};
};

TEST_F(integer_histogram_test_constructed_t, dump)
{
    auto const expected = dump_t{{-4, 13}, {-2, 7}, {-1, 3}, {0, 2}, {1, 5}, {3, 11}, {4, 17}, {5, 19}};

    auto const actual = dump(sut);

    EXPECT_EQ(expected, actual);
    EXPECT_EQ(77, sut.count());
}

TEST_F(integer_histogram_test_constructed_t, copy)
{
    auto const copy = sut;
    EXPECT_EQ(sut, copy);
}

TEST_F(integer_histogram_test_constructed_t, modified_copy)
{
    auto modified_copy = sut;
    modified_copy.sample(1000);
    EXPECT_NE(sut, modified_copy);
    EXPECT_EQ(sut.count() + 1, modified_copy.count());
}

TEST_F(integer_histogram_test_constructed_t, ostream_inserter)
{
    auto const expected = "{{-4, 13}, {-2, 7}, {-1, 3}, {0, 2}, {1, 5}, {3, 11}, {4, 17}, {5, 19}}";

    auto actual = std::ostringstream{};
    actual << sut;

    EXPECT_EQ(expected, actual.str());
}

// ====================================================================================================================
// Percentiles
// ====================================================================================================================

struct percentile_calculator_test_t : Test
{
    using histogram_t = histogram_t<int_t>;
    using sut_t       = percentile_calculator_t<int_t, histogram_t>;
    using result_t    = sut_t::result_t;
    sut_t sut{};
};

TEST_F(percentile_calculator_test_t, empty)
{
    auto const expected = result_t{};

    auto const actual = sut(histogram_t{});

    EXPECT_EQ(expected, actual);
}

TEST_F(percentile_calculator_test_t, segments_1)
{
    auto const expected = result_t{.p50 = 10, .p90 = 10, .p95 = 10, .p99 = 10, .p100 = 10};

    auto histogram = histogram_t{};

    histogram.sample(10);

    auto const actual = sut(histogram);

    EXPECT_EQ(expected, actual);
}

TEST_F(percentile_calculator_test_t, segments_2)
{
    auto const expected = result_t{.p50 = -10, .p90 = 10, .p95 = 10, .p99 = 10, .p100 = 10};

    auto histogram = histogram_t{};

    histogram.sample(-10);
    histogram.sample(10);

    auto const actual = sut(histogram);

    EXPECT_EQ(expected, actual);
}

TEST_F(percentile_calculator_test_t, segments_10)
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

TEST_F(percentile_calculator_test_t, segments_20)
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

TEST_F(percentile_calculator_test_t, segments_100)
{
    auto const expected = result_t{.p50 = 50, .p90 = 90, .p95 = 95, .p99 = 99, .p100 = 100};

    auto histogram = histogram_t{};
    for (auto value = 1; value <= 100; ++value) histogram.sample(value);

    auto const actual = sut(histogram);

    EXPECT_EQ(expected, actual);
}

TEST_F(percentile_calculator_test_t, all_in_one_bin)
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

TEST_F(percentile_calculator_test_t, sparse_step)
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

struct percentile_calculator_fuzz_test_t : percentile_calculator_test_t
{
    using data_t = std::vector<int_t>;
    struct oracle_t
    {
        auto operator()(data_t data) const noexcept -> result_t
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

TEST_F(percentile_calculator_fuzz_test_t, fuzz)
{
    fuzz();
}

// ====================================================================================================================
// Distribution
// ====================================================================================================================

struct distribution_test_t : Test
{
    using value_t = int_t;

    struct mock_histogram_t
    {
        MOCK_METHOD(void, sample, (int_t value));
        virtual ~mock_histogram_t() = default;
    };
    StrictMock<mock_histogram_t> mock_histogram;

    struct histogram_t
    {
        std::string_view  name;
        mock_histogram_t* mock = nullptr;

        auto sample(int_t value) -> void { mock->sample(value); }

        friend auto operator<<(std::ostream& out, histogram_t const& src) -> std::ostream& { return out << src.name; }
    };

    using percentile_result_t                        = int_t;
    static constexpr auto expected_percentile_result = percentile_result_t{17};

    struct mock_percentile_calculator_t
    {
        MOCK_METHOD(percentile_result_t, call, (mock_histogram_t const& mock_histogram), (const, noexcept));
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

} // namespace
} // namespace crv
