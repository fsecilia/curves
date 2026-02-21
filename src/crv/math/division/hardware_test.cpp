// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include "hardware.hpp"
#include <crv/math/io.hpp>
#include <crv/test/test.hpp>
#include <ostream>
#include <string>

namespace crv {
namespace {

struct division_param_t
{
    std::string    name;
    uint128_t      dividend;
    uint64_t       divisor;
    div_u128_u64_t result;

    friend auto operator<<(std::ostream& out, division_param_t const& src) -> std::ostream&
    {
        return out << "{.name = \"" << src.name << "\", .dividend = " << src.dividend << ", .divisor = " << src.divisor
                   << ", result = " << src.result << "}";
    }
};

struct math_division_test_t : TestWithParam<division_param_t>
{
    uint128_t             dividend = GetParam().dividend;
    uint64_t              divisor  = GetParam().divisor;
    div_u128_u64_t const& result   = GetParam().result;

    auto test(auto const&& calc_actual) noexcept -> void { EXPECT_EQ(result, calc_actual(dividend, divisor)); }
};

TEST_P(math_division_test_t, intended_implementation)
{
    test(&div_u128_u64);
}

TEST_P(math_division_test_t, generic_implementation)
{
    test(&div_u128_u64_generic);
}

#if defined __x86_64__

TEST_P(math_division_test_t, x64_implementation)
{
    test(&div_u128_u64_x64);
}

#endif

// constructs uint128_t from individual words
constexpr auto u128(uint64_t high, uint64_t low) -> uint128_t
{
    return (static_cast<uint128_t>(high) << 64) | low;
}

constexpr auto max = std::numeric_limits<uint64_t>::max();

division_param_t const division_params[] = {
    // basics
    {"0/1", 0, 1, {.quotient = 0, .remainder = 0}},
    {"1/1", 1, 1, {.quotient = 1, .remainder = 0}},
    {"2/1", 2, 1, {.quotient = 2, .remainder = 0}},
    {"1/2", 1, 2, {.quotient = 0, .remainder = 1}},
    {"2/2", 2, 2, {.quotient = 1, .remainder = 0}},
    {"3/2", 3, 2, {.quotient = 1, .remainder = 1}},
    {"small/small", 100, 3, {.quotient = 33, .remainder = 1}},

    /*
        max dividend with divisor = 1

        This is the largest dividend that will not trap when the divisor is 1.
    */
    {"max/1", max, 1, {.quotient = max, .remainder = 0}},

    /*
        high bit set in dividend with divisor = 2

        This sets the high bit in the result.
    */
    {"high bit set", u128(1, 0), 2, {.quotient = 1ULL << 63, .remainder = 0}},

    /*
        max possible remainder

        This sets all bits in the remainder. Dividend is one less than a clean division.
    */
    {"max remainder", u128(0, max - 1), max, {.quotient = 0, .remainder = max - 1}},

    /*
        max everything

        Dividend is 127 set bits. Divisor is all set bits. This is the largest division that can be performed without
        trapping. It exercises the full width of the ALU.
    */
    {"max everything", u128(max - 1, max), max, {.quotient = max, .remainder = max - 1}},
};
INSTANTIATE_TEST_SUITE_P(cases, math_division_test_t, ValuesIn(division_params),
                         test_name_generator_t<division_param_t>{});

} // namespace
} // namespace crv
