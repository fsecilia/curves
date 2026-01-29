// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include "gtest/gtest.h"
#include <crv/math/integer.hpp>
#include <crv/math/io.hpp>
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <crv/test/typed_equal.hpp>
#include <ostream>
#include <string>

namespace crv {
namespace {

// ====================================================================================================================
// Math
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// division
// --------------------------------------------------------------------------------------------------------------------

namespace division {

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

} // namespace division

// --------------------------------------------------------------------------------------------------------------------
// log2
// --------------------------------------------------------------------------------------------------------------------

static_assert(log2<uint8_t>(1) == 0);
static_assert(log2<uint8_t>(max<uint8_t>()) == 7);

// bottom of valid range
static_assert(log2<uint64_t>((1ULL << 0) + 0) == 0);
static_assert(log2<uint64_t>((1ULL << 0) + 1) == 1);

static_assert(log2<uint64_t>((1ULL << 1) - 1) == 0);
static_assert(log2<uint64_t>((1ULL << 1) + 0) == 1);
static_assert(log2<uint64_t>((1ULL << 1) + 1) == 1);

static_assert(log2<uint64_t>((1ULL << 2) - 1) == 1);
static_assert(log2<uint64_t>((1ULL << 2) + 0) == 2);
static_assert(log2<uint64_t>((1ULL << 2) + 1) == 2);

static_assert(log2<uint64_t>((1ULL << 3) - 1) == 2);
static_assert(log2<uint64_t>((1ULL << 3) + 0) == 3);
static_assert(log2<uint64_t>((1ULL << 3) + 1) == 3);

// top of valid range
static_assert(log2<uint64_t>((1ULL << 62) - 1) == 61);
static_assert(log2<uint64_t>((1ULL << 62) + 0) == 62);
static_assert(log2<uint64_t>((1ULL << 62) + 1) == 62);

static_assert(log2<uint64_t>((1ULL << 63) - 1) == 62);
static_assert(log2<uint64_t>((1ULL << 63) + 0) == 63);
static_assert(log2<uint64_t>((1ULL << 63) + 1) == 63);

// max boundary
static_assert(log2<uint64_t>(max<uint64_t>()) == 63);

#if !defined NDEBUG

TEST(log2, asserts_on_log2_0)
{
    EXPECT_DEATH(log2(0u), "log2: domain error");
}

#endif

// ====================================================================================================================
// Conversions
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// int_cast
// --------------------------------------------------------------------------------------------------------------------

static_assert(int_cast<int8_t>(int16_t{min<int8_t>()}) == min<int8_t>());
static_assert(int_cast<int8_t>(int16_t{-1}) == -1);
static_assert(int_cast<int8_t>(int16_t{0}) == 0);
static_assert(int_cast<int8_t>(int16_t{1}) == 1);
static_assert(int_cast<int8_t>(int16_t{max<int8_t>()}) == max<int8_t>());

static_assert(int_cast<int8_t>(uint16_t{0}) == 0);
static_assert(int_cast<int8_t>(uint16_t{1}) == 1);
static_assert(int_cast<int8_t>(uint16_t{max<int8_t>()}) == max<int8_t>());

static_assert(int_cast<int16_t>(int8_t{min<int8_t>()}) == min<int8_t>());
static_assert(int_cast<int16_t>(int8_t{-1}) == -1);
static_assert(int_cast<int16_t>(int8_t{0}) == 0);
static_assert(int_cast<int16_t>(int8_t{1}) == 1);
static_assert(int_cast<int16_t>(int8_t{max<int8_t>()}) == max<int8_t>());

static_assert(int_cast<int16_t>(uint8_t{0}) == 0);
static_assert(int_cast<int16_t>(uint8_t{1}) == 1);
static_assert(int_cast<int16_t>(uint8_t{max<int8_t>()}) == max<int8_t>());

#if !defined NDEBUG

TEST(int_cast, asserts_casting_negative_to_unsigned)
{
    EXPECT_DEATH(int_cast<uint8_t>(-1), "int_cast: input out of range");
}

TEST(int_cast, asserts_casting_oor)
{
    EXPECT_DEATH(int_cast<int8_t>(max<int8_t>() + 1), "int_cast: input out of range");
}

#endif

// --------------------------------------------------------------------------------------------------------------------
// to_unsigned_abs
// --------------------------------------------------------------------------------------------------------------------

static_assert(typed_equal<uint64_t>(to_unsigned_abs(min<int64_t>()), static_cast<uint64_t>(max<int64_t>()) + 1));
static_assert(typed_equal<uint32_t>(to_unsigned_abs(min<int32_t>()), static_cast<uint32_t>(max<int32_t>()) + 1));
static_assert(typed_equal<unsigned>(to_unsigned_abs(-1), static_cast<unsigned>(1)));
static_assert(typed_equal<unsigned>(to_unsigned_abs(0), static_cast<unsigned>(0)));
static_assert(typed_equal<unsigned>(to_unsigned_abs(1), static_cast<unsigned>(1)));
static_assert(typed_equal<uint32_t>(to_unsigned_abs(max<int32_t>()), static_cast<uint32_t>(max<int32_t>())));
static_assert(typed_equal<uint64_t>(to_unsigned_abs(max<int64_t>()), static_cast<uint64_t>(max<int64_t>())));

// --------------------------------------------------------------------------------------------------------------------
// to_signed_copysign
// --------------------------------------------------------------------------------------------------------------------

static_assert(typed_equal<int64_t>(to_signed_copysign(static_cast<uint64_t>(max<int64_t>()) + 1, -1), min<int64_t>()));
static_assert(typed_equal<int64_t>(to_signed_copysign(static_cast<uint64_t>(max<int64_t>()), -2), -max<int64_t>()));
static_assert(typed_equal<int32_t>(to_signed_copysign(static_cast<uint32_t>(max<int32_t>()) + 1, -3), min<int32_t>()));
static_assert(typed_equal<int32_t>(to_signed_copysign(static_cast<uint32_t>(max<int32_t>()), -5), -max<int32_t>()));
static_assert(typed_equal<int>(to_signed_copysign(static_cast<unsigned>(1), -7), -1));
static_assert(typed_equal<int>(to_signed_copysign(static_cast<unsigned>(0), -11), 0));
static_assert(typed_equal<int>(to_signed_copysign(static_cast<unsigned>(0), 0), 0));
static_assert(typed_equal<int>(to_signed_copysign(static_cast<unsigned>(1), 1), 1));
static_assert(typed_equal<int32_t>(to_signed_copysign(static_cast<uint32_t>(max<int32_t>()), 2), max<int32_t>()));
static_assert(typed_equal<int64_t>(to_signed_copysign(static_cast<uint64_t>(max<int64_t>()), 5), max<int64_t>()));

#if !defined NDEBUG

TEST(to_signed_copysign, asserts_casting_below_min)
{
    EXPECT_DEATH(to_signed_copysign<uint8_t>(max<int8_t>() + 2, -1), "to_signed_copysign: input out of range");
}

TEST(to_signed_copysign, asserts_casting_above_max)
{
    EXPECT_DEATH(to_signed_copysign<uint8_t>(max<int8_t>() + 1, 1), "to_signed_copysign: input out of range");
}

#endif

} // namespace
} // namespace crv
