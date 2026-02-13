// SPDX-License-Identifier: MIT
/*!
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include <crv/lib.hpp>
#include <crv/math/stats.hpp>
#include <crv/test/test.hpp>
#include <algorithm>
#include <map>
#include <random>

namespace crv {
namespace {

struct percentile_calculator_fuzz_test_t : Test
{
    using histogram_t = histogram_t<int_t>;
    using sut_t       = percentile_calculator_t<int_t, histogram_t>;
    using result_t    = sut_t::result_t;
    sut_t sut{};

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

} // namespace
} // namespace crv
