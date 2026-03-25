// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "hardware_divider.hpp"
#include <crv/math/division/io.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <limits>

namespace crv::division {
namespace {

// compile-time check to ensure the generic implementation remains constexpr-friendly
static_assert(hardware_divider_t<uint8_t>{}(100, 3).quotient == 33);

template <typename t_narrow_t> struct hardware_divider_test_t : Test
{
    using narrow_t = t_narrow_t;
    using wide_t   = wider_t<narrow_t>;
    using result_t = result_t<narrow_t>;
    using sut_t    = hardware_divider_t<narrow_t>;

    // constructs wide dividend from individual words
    static constexpr auto make_dividend(narrow_t high, narrow_t low) -> wide_t
    {
        constexpr auto shift = std::numeric_limits<narrow_t>::digits;
        return (int_cast<wide_t>(high) << shift) | low;
    }
};

// test using all narrow sizes; this tests specialized implementations and the generic implementation
using narrow_types_t = Types<uint8_t, uint16_t, uint32_t, uint64_t>;
TYPED_TEST_SUITE(hardware_divider_test_t, narrow_types_t);

// basic division results
TYPED_TEST(hardware_divider_test_t, basics)
{
    using result_t = TestFixture::result_t;
    using sut_t    = TestFixture::sut_t;

    auto const sut = sut_t{};

    EXPECT_EQ((result_t{0, 0}), sut(0, 1));
    EXPECT_EQ((result_t{1, 0}), sut(1, 1));
    EXPECT_EQ((result_t{2, 0}), sut(2, 1));
    EXPECT_EQ((result_t{0, 1}), sut(1, 2));
    EXPECT_EQ((result_t{1, 0}), sut(2, 2));
    EXPECT_EQ((result_t{1, 1}), sut(3, 2));
    EXPECT_EQ((result_t{33, 1}), sut(100, 3));
}

// largest dividend that will not trap when divisor is 1
TYPED_TEST(hardware_divider_test_t, max_dividend_divisor_one)
{
    using narrow_t = TestFixture::narrow_t;
    using result_t = TestFixture::result_t;
    using sut_t    = TestFixture::sut_t;

    auto const sut = sut_t{};

    constexpr auto expected = result_t{max<narrow_t>(), 0};
    auto const     actual   = sut(max<narrow_t>(), 1);

    EXPECT_EQ(expected, actual);
}

// sets the high bit in the result
TYPED_TEST(hardware_divider_test_t, high_bit_set)
{
    using narrow_t = TestFixture::narrow_t;
    using result_t = TestFixture::result_t;
    using sut_t    = TestFixture::sut_t;

    auto const sut      = sut_t{};
    auto const dividend = TestFixture::make_dividend(1, 0);

    constexpr auto expected = result_t{narrow_t{1} << (std::numeric_limits<narrow_t>::digits - 1), 0};
    auto const     actual   = sut(dividend, 2);

    EXPECT_EQ(expected, actual);
}

// sets all bits in the remainder; dividend is one less than a clean division
TYPED_TEST(hardware_divider_test_t, max_remainder)
{
    using narrow_t = TestFixture::narrow_t;
    using result_t = TestFixture::result_t;
    using sut_t    = TestFixture::sut_t;

    auto const sut      = sut_t{};
    auto const dividend = TestFixture::make_dividend(0, max<narrow_t>() - 1);

    auto const expected = result_t{0, max<narrow_t>() - 1};
    auto const actual   = sut(dividend, max<narrow_t>());

    EXPECT_EQ(expected, actual);
}

// dividend is all set bits except high bit, divisor is all set bits; exercises the full width of ALU
TYPED_TEST(hardware_divider_test_t, max_everything)
{
    using narrow_t = TestFixture::narrow_t;
    using result_t = TestFixture::result_t;
    using sut_t    = TestFixture::sut_t;

    auto const sut      = sut_t{};
    auto const dividend = TestFixture::make_dividend(max<narrow_t>() - 1, max<narrow_t>());

    auto const expected = result_t{max<narrow_t>(), max<narrow_t>() - 1};
    auto const actual   = sut(dividend, max<narrow_t>());

    EXPECT_EQ(expected, actual);
}

} // namespace
} // namespace crv::division
