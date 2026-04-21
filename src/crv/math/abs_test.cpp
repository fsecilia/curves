// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "abs.hpp"
#include <crv/test/test.hpp>
#include <cmath>
#include <limits>

namespace crv {
namespace {

// ====================================================================================================================
// Compile-Time Path
// ====================================================================================================================

// float
static_assert(abs(-42.5) == 42.5);
static_assert(abs(42.5) == 42.5);
static_assert(abs(-0.0) == -0.0);
static_assert(abs(0.0) == 0.0);

// integer
static_assert(abs(-100) == 100);
static_assert(abs(100) == 100);

// ====================================================================================================================
// Runtime Path
// ====================================================================================================================

struct abs_test_t : Test
{};

TEST_F(abs_test_t, preserves_qnan)
{
    constexpr auto qnan = std::numeric_limits<float_t>::quiet_NaN();
    EXPECT_TRUE(std::isnan(abs(-qnan)));
    EXPECT_TRUE(std::isnan(abs(qnan)));
}

TEST_F(abs_test_t, preserves_snan)
{
    constexpr auto snan = std::numeric_limits<float_t>::signaling_NaN();
    EXPECT_TRUE(std::isnan(abs(-snan)));
    EXPECT_TRUE(std::isnan(abs(snan)));
}

// --------------------------------------------------------------------------------------------------------------------
// Parameterized Test
// --------------------------------------------------------------------------------------------------------------------

struct test_vector_t
{
    float_t input;
    float_t expected;

    friend auto operator<<(std::ostream& out, test_vector_t const& src) -> std::ostream&
    {
        return out << "{.input = " << src.input << ", .expected = " << src.expected << "}";
    }
};

struct abs_vector_test_t : abs_test_t, WithParamInterface<test_vector_t>
{
    float_t input = GetParam().input;
    float_t expected = GetParam().expected;
};

TEST_P(abs_vector_test_t, result)
{
    using crv::abs;

    auto const actual = abs(input);

    EXPECT_EQ(expected, actual);
}

constexpr auto inf = std::numeric_limits<float_t>::infinity();
constexpr auto dmin = std::numeric_limits<float_t>::denorm_min();
constexpr auto min = std::numeric_limits<float_t>::min();
constexpr auto max = std::numeric_limits<float_t>::max();

// clang-format off
constexpr test_vector_t vectors[] = {
    {-42.5, 42.5},
    { 42.5, 42.5},
    { -0.0,  0.0},
    {  0.0,  0.0},
    {-dmin, dmin},
    { dmin, dmin},
    { -min,  min},
    {  min,  min},
    { -max,  max},
    {  max,  max},
    { -inf,  inf},
    {  inf,  inf},
};
INSTANTIATE_TEST_SUITE_P(vectors, abs_vector_test_t, ValuesIn(vectors));
// clang-format on

} // namespace
} // namespace crv
