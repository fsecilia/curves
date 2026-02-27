// SPDX-License-Identifier: MIT
/*!
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include "io.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <sstream>
#include <string>

namespace crv {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// uint128_t
// --------------------------------------------------------------------------------------------------------------------

struct uint128_param_t
{
    uint128_t   number;
    std::string string;
};

struct math_io_uint128_test_t : TestWithParam<uint128_param_t>
{};

TEST_P(math_io_uint128_test_t, result)
{
    auto const expected = GetParam().string;

    std::ostringstream out;
    out << GetParam().number;
    auto const actual = out.str();

    EXPECT_EQ(expected, actual);
}

uint128_param_t const uint128_t_test_params[] = {
    {0, "0"},
    {1, "1"},
    {9, "9"},
    {10, "10"},
    {11, "11"},
    {99, "99"},
    {100, "100"},
    {101, "101"},
    {max<int64_t>(), "9223372036854775807"},
    {max<uint64_t>(), "18446744073709551615"},
    {max<int128_t>(), "170141183460469231731687303715884105727"},
    {max<uint128_t>(), "340282366920938463463374607431768211455"},
};
INSTANTIATE_TEST_SUITE_P(cases, math_io_uint128_test_t, ValuesIn(uint128_t_test_params));

// --------------------------------------------------------------------------------------------------------------------
// int128_t
// --------------------------------------------------------------------------------------------------------------------

struct int128_param_t
{
    int128_t    number;
    std::string string;
};

struct math_io_int128_test_t : TestWithParam<int128_param_t>
{};

TEST_P(math_io_int128_test_t, result)
{
    auto const expected = GetParam().string;

    std::ostringstream out;
    out << GetParam().number;
    auto const actual = out.str();

    EXPECT_EQ(expected, actual);
}

int128_param_t const s128_test_params[] = {
    {min<int128_t>(), "-170141183460469231731687303715884105728"},
    {-static_cast<int128_t>(max<uint64_t>()), "-18446744073709551615"},
    {min<int64_t>(), "-9223372036854775808"},
    {-101, "-101"},
    {-100, "-100"},
    {-99, "-99"},
    {-11, "-11"},
    {-10, "-10"},
    {-9, "-9"},
    {-1, "-1"},

    {0, "0"},

    {1, "1"},
    {9, "9"},
    {10, "10"},
    {11, "11"},
    {99, "99"},
    {100, "100"},
    {101, "101"},
    {max<int64_t>(), "9223372036854775807"},
    {max<uint64_t>(), "18446744073709551615"},
    {max<int128_t>(), "170141183460469231731687303715884105727"},
};
INSTANTIATE_TEST_SUITE_P(cases, math_io_int128_test_t, ValuesIn(s128_test_params));

} // namespace

// --------------------------------------------------------------------------------------------------------------------
// result_t
// --------------------------------------------------------------------------------------------------------------------

namespace division {
namespace {

struct division_result_param_t
{
    division::result_t<uint64_t, uint32_t> sut;
    std::string                            expected;
};

struct math_io_division_result_test_t : TestWithParam<division_result_param_t>
{};

TEST_P(math_io_division_result_test_t, result)
{
    auto const expected = GetParam().expected;

    std::ostringstream out;
    out << GetParam().sut;
    auto const actual = out.str();

    EXPECT_EQ(expected, actual);
}

division_result_param_t const division_result_test_params[] = {
    {{.quotient = 0, .remainder = 0}, "{.quotient = 0, .remainder = 0}"},
    {{.quotient = 0, .remainder = 1}, "{.quotient = 0, .remainder = 1}"},
    {{.quotient = 1, .remainder = 0}, "{.quotient = 1, .remainder = 0}"},
    {{.quotient = 1, .remainder = 1}, "{.quotient = 1, .remainder = 1}"},

    {{.quotient = 0, .remainder = max<uint32_t>()}, "{.quotient = 0, .remainder = 4294967295}"},
    {{.quotient = max<uint64_t>(), .remainder = 0}, "{.quotient = 18446744073709551615, .remainder = 0}"},

    {
        {
            .quotient  = max<uint64_t>(),
            .remainder = max<uint32_t>(),
        },
        "{.quotient = 18446744073709551615, .remainder = 4294967295}",
    },
};
INSTANTIATE_TEST_SUITE_P(cases, math_io_division_result_test_t, ValuesIn(division_result_test_params));

} // namespace
} // namespace division
} // namespace crv
