// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "abs.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <cmath>

namespace crv {
namespace {

// ====================================================================================================================
// compile-time path
// ====================================================================================================================

// float
static_assert(abs(-42.5) == 42.5);
static_assert(abs(42.5) == 42.5);
static_assert(abs(-0.0) == -0.0);
static_assert(abs(0.0) == 0.0);

// integer
static_assert(abs(-100) == 100);
static_assert(abs(100) == 100);

// int boundaries
static_assert(abs(max<int32_t>()) == max<int32_t>());
static_assert(abs(-max<int32_t>()) == max<int32_t>());

// 128-bit types
static_assert(crv::abs(static_cast<int128_t>(-100)) == static_cast<int128_t>(100));

// ====================================================================================================================
// runtime path
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// integers
// --------------------------------------------------------------------------------------------------------------------

struct int_test_vector_t
{
    int32_t input;
    int32_t expected;

    friend auto operator<<(std::ostream& out, int_test_vector_t const& src) -> std::ostream&
    {
        return out << "{.input = " << src.input << ", .expected = " << src.expected << "}";
    }
};

struct abs_int_vector_test_t : Test, WithParamInterface<int_test_vector_t>
{
    int32_t input = GetParam().input;
    int32_t expected = GetParam().expected;
};

TEST_P(abs_int_vector_test_t, result)
{
    using crv::abs;
    EXPECT_EQ(expected, abs(input));
}

constexpr int_test_vector_t int_vectors[] = {
    {-100, 100},
    {100, 100},
    {0, 0},
    {-std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::max()},
    {std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::max()},
};
INSTANTIATE_TEST_SUITE_P(int_vectors, abs_int_vector_test_t, ValuesIn(int_vectors));

// --------------------------------------------------------------------------------------------------------------------
// floating point
// --------------------------------------------------------------------------------------------------------------------

struct abs_float_test_t : Test
{};

TEST_F(abs_float_test_t, preserves_qnan)
{
    constexpr auto qnan = std::numeric_limits<float_t>::quiet_NaN();
    EXPECT_TRUE(std::isnan(abs(-qnan)));
    EXPECT_TRUE(std::isnan(abs(qnan)));
}

TEST_F(abs_float_test_t, preserves_snan)
{
    constexpr auto snan = std::numeric_limits<float_t>::signaling_NaN();
    EXPECT_TRUE(std::isnan(abs(-snan)));
    EXPECT_TRUE(std::isnan(abs(snan)));
}

// --------------------------------------------------------------------------------------------------------------------
// vector test
// --------------------------------------------------------------------------------------------------------------------

struct float_test_vector_t
{
    float_t input;
    float_t expected;

    friend auto operator<<(std::ostream& out, float_test_vector_t const& src) -> std::ostream&
    {
        return out << "{.input = " << src.input << ", .expected = " << src.expected << "}";
    }
};

struct abs_float_vector_test_t : abs_float_test_t, WithParamInterface<float_test_vector_t>
{
    float_t input = GetParam().input;
    float_t expected = GetParam().expected;
};

TEST_P(abs_float_vector_test_t, result)
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
constexpr float_test_vector_t vectors[] = {
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
INSTANTIATE_TEST_SUITE_P(vectors, abs_float_vector_test_t, ValuesIn(vectors));
// clang-format on

} // namespace
} // namespace crv
