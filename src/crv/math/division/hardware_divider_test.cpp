// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "hardware_divider.hpp"
#include <crv/math/division/io.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/test/test.hpp>
#include <limits>

namespace crv::division {
namespace {

// compile-time check to ensure the generic implementation remains constexpr-friendly
static_assert(hardware_divider_t<uint16_t, uint8_t>{}(100, 3).quotient == 33);

template <typename t_narrow_t> struct hardware_divider_asm_test_t : ::testing::Test
{
    using narrow_t = t_narrow_t;
    using wide_t   = wider_t<narrow_t>;
    using result_t = result_t<narrow_t>;
    using sut_t    = hardware_divider_t<wide_t, narrow_t>;

    // constructs wide dividend from individual words
    static constexpr auto make_dividend(narrow_t high, narrow_t low) -> wide_t
    {
        constexpr auto shift = std::numeric_limits<narrow_t>::digits;
        return (static_cast<wide_t>(high) << shift) | low;
    }

    static constexpr auto max = std::numeric_limits<narrow_t>::max();
};

using AsmDivisorTypes = ::testing::Types<uint8_t, uint16_t, uint32_t, uint64_t>;
TYPED_TEST_SUITE(hardware_divider_asm_test_t, AsmDivisorTypes);

TYPED_TEST(hardware_divider_asm_test_t, basics)
{
    using result_t = typename TestFixture::result_t;

    auto const sut = typename TestFixture::sut_t{};

    // basic division results
    EXPECT_EQ((result_t{0, 0}), sut(0, 1));
    EXPECT_EQ((result_t{1, 0}), sut(1, 1));
    EXPECT_EQ((result_t{2, 0}), sut(2, 1));
    EXPECT_EQ((result_t{0, 1}), sut(1, 2));
    EXPECT_EQ((result_t{1, 0}), sut(2, 2));
    EXPECT_EQ((result_t{1, 1}), sut(3, 2));
    EXPECT_EQ((result_t{33, 1}), sut(100, 3));
}

TYPED_TEST(hardware_divider_asm_test_t, max_dividend_divisor_one)
{
    using result_t = typename TestFixture::result_t;

    auto const sut = typename TestFixture::sut_t{};

    // largest dividend that will not trap when divisor is 1
    EXPECT_EQ((result_t{TestFixture::max, 0}), sut(TestFixture::max, 1));
}

TYPED_TEST(hardware_divider_asm_test_t, high_bit_set)
{
    using narrow_t = typename TestFixture::narrow_t;
    using result_t = typename TestFixture::result_t;

    auto const sut = typename TestFixture::sut_t{};

    // sets the high bit in the result
    constexpr narrow_t expected_quotient = narrow_t{1} << (std::numeric_limits<narrow_t>::digits - 1);
    auto const         dividend          = TestFixture::make_dividend(1, 0);

    EXPECT_EQ((result_t{expected_quotient, 0}), sut(dividend, 2));
}

TYPED_TEST(hardware_divider_asm_test_t, max_remainder)
{
    using result_t = typename TestFixture::result_t;

    auto const sut = typename TestFixture::sut_t{};

    // sets all bits in the remainder; dividend is one less than a clean division
    auto const dividend = TestFixture::make_dividend(0, TestFixture::max - 1);

    EXPECT_EQ((result_t{0, TestFixture::max - 1}), sut(dividend, TestFixture::max));
}

TYPED_TEST(hardware_divider_asm_test_t, max_everything)
{
    using result_t = typename TestFixture::result_t;

    auto const sut = typename TestFixture::sut_t{};

    // dividend is all set bits except high bit, divisor is all set bits; exercises the full width of ALU
    auto const dividend = TestFixture::make_dividend(TestFixture::max - 1, TestFixture::max);

    EXPECT_EQ((result_t{TestFixture::max, TestFixture::max - 1}), sut(dividend, TestFixture::max));
}

} // namespace
} // namespace crv::division
