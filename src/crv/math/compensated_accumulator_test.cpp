// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "compensated_accumulator.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>

namespace crv {
namespace {

struct compensated_accumulator_test_t : Test
{
    // use float32_t so it drifts sooner than float64_t
    using scalar_t = float32_t;
    using sut_t = compensated_accumulator_t<scalar_t>;

    static constexpr auto iterations = int_t{1'000'000};
};

TEST_F(compensated_accumulator_test_t, sums_normally)
{
    auto sut = sut_t{};

    for (auto i = 0; i < iterations; ++i) sut += static_cast<scalar_t>(i);

    EXPECT_EQ(static_cast<float>(iterations * (iterations - 1)) / 2, static_cast<float>(sut));
}

TEST_F(compensated_accumulator_test_t, catches_vanishing_udates)
{
    auto const large_value = scalar_t{1.0};
    auto const small_value = std::numeric_limits<scalar_t>::epsilon() / 10;
    auto const expected_change = small_value * iterations;

    auto reference = large_value;
    auto sut = sut_t{large_value};
    for (int i = 0; i < iterations; ++i)
    {
        reference += small_value;
        sut += small_value;
    }

    EXPECT_EQ(reference, large_value);
    EXPECT_EQ(static_cast<float>(sut), large_value + expected_change);
}

TEST_F(compensated_accumulator_test_t, final_conversion_includes_compensation)
{
    auto const sut = sut_t{.sum = scalar_t{1}, .compensation = scalar_t{2}};
    EXPECT_DOUBLE_EQ(sut.sum, 1.0);
    EXPECT_DOUBLE_EQ(static_cast<scalar_t>(sut), 3.0);
}

} // namespace
} // namespace crv
