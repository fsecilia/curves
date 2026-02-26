// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include "hardware_divider.hpp"
#include <crv/math/int_traits.hpp>
#include <crv/math/io.hpp>
#include <crv/test/test.hpp>
#include <climits>
#include <limits>
#include <ostream>
#include <string>

namespace crv::division {
namespace {

template <unsigned_integral dividend_t, unsigned_integral divisor_t>
constexpr auto test_division(result_t<divisor_t> expected, dividend_t dividend, divisor_t divisor) noexcept -> bool
{
    return expected == hardware_divider_t<dividend_t, divisor_t>{}(dividend, divisor);
};

template <typename quotient_t> constexpr auto test() noexcept -> void
{
    using dividend_t = wider_t<quotient_t>;
    using divisor_t  = quotient_t;
    using result_t   = result_t<quotient_t>;

    auto const test = [](result_t const& expected, dividend_t dividend, divisor_t divisor) noexcept {
        return test_division<wider_t<quotient_t>, quotient_t>(expected, dividend, divisor);
    };

    // basics
    static_assert(test({0, 0}, 0, 1));
    static_assert(test({1, 0}, 1, 1));
    static_assert(test({2, 0}, 2, 1));
    static_assert(test({3, 0}, 3, 1));
    static_assert(test({0, 0}, 0, 2));
    static_assert(test({0, 1}, 1, 2));
    static_assert(test({1, 0}, 2, 2));
    static_assert(test({1, 1}, 3, 2));
    static_assert(test({33, 1}, 100, 3));

    constexpr auto max      = std::numeric_limits<quotient_t>::max();
    constexpr auto size     = static_cast<quotient_t>(sizeof(quotient_t) * CHAR_BIT);
    constexpr auto sign_bit = std::numeric_limits<quotient_t>::digits;

    // constructs double-width dividend
    auto dividend
        = [](quotient_t high, quotient_t low) -> dividend_t { return (static_cast<dividend_t>(high) << size) | low; };

    /*
        max dividend with divisor = 1

        This is the largest dividend that will not trap when the divisor is 1.
    */
    static_assert(test({max, 0}, max, 1));

    /*
        high bit set in dividend with divisor = 2

        This sets the high bit in the result.
    */
    static_assert(test({quotient_t{1} << (sign_bit - 1), 0}, dividend(1, 0), 2));

    /*
        max possible remainder

        This sets all bits in the remainder. Dividend is one less than a clean division.
    */
    static_assert(test({0, max - 1}, dividend(0, max - 1), max));

    /*
        max everything

        Dividend is all set bits except high bit. Divisor is all set bits. This is the largest division that can be
        performed without trapping. It exercises the full width of the ALU.
    */
    static_assert(test({max, max - 1}, dividend(max - 1, max), max));
};

constexpr auto test() noexcept -> void
{
    test<uint8_t>();
    test<uint16_t>();
    test<uint32_t>();
}

// --------------------------------------------------------------------------------------------------------------------
// u128/u64 specifics
//
// These cannot be made constexpr because of the inline asm, so they are tested with a table.
// --------------------------------------------------------------------------------------------------------------------

struct hardware_divider_u128_u64_test_param_t
{
    std::string        name;
    uint128_t          dividend;
    uint64_t           divisor;
    result_t<uint64_t> result;

    friend auto operator<<(std::ostream& out, hardware_divider_u128_u64_test_param_t const& src) -> std::ostream&
    {
        return out << "{.name = \"" << src.name << "\", .dividend = " << src.dividend << ", .divisor = " << src.divisor
                   << ", result = " << src.result << "}";
    }
};

struct division_hardware_divider_u128_u64_test_t : TestWithParam<hardware_divider_u128_u64_test_param_t>
{
    uint128_t          dividend = GetParam().dividend;
    uint64_t           divisor  = GetParam().divisor;
    result_t<uint64_t> result   = GetParam().result;

    using sut_t = hardware_divider_t<uint128_t, uint64_t>;
    sut_t sut{};
};

TEST_P(division_hardware_divider_u128_u64_test_t, result)
{
    EXPECT_EQ(result, sut(dividend, divisor));
}

// constructs uint128_t from individual words
constexpr auto dividend(uint64_t high, uint64_t low) -> uint128_t
{
    return (static_cast<uint128_t>(high) << 64) | low;
}

constexpr auto max = std::numeric_limits<uint64_t>::max();

hardware_divider_u128_u64_test_param_t const division_params[] = {
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
    {"high bit set", dividend(1, 0), 2, {.quotient = 1ULL << 63, .remainder = 0}},

    /*
        max possible remainder

        This sets all bits in the remainder. Dividend is one less than a clean division.
    */
    {"max remainder", dividend(0, max - 1), max, {.quotient = 0, .remainder = max - 1}},

    /*
        max everything

        Dividend is 127 set bits. Divisor is all set bits. This is the largest division that can be performed without
        trapping. It exercises the full width of the ALU.
    */
    {"max everything", dividend(max - 1, max), max, {.quotient = max, .remainder = max - 1}},
};
INSTANTIATE_TEST_SUITE_P(cases, division_hardware_divider_u128_u64_test_t, ValuesIn(division_params),
                         test_name_generator_t<hardware_divider_u128_u64_test_param_t>{});

} // namespace
} // namespace crv::division
