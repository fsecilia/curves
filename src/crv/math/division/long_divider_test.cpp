// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include "long_divider.hpp"
#include <crv/math/io.hpp>
#include <crv/test/test.hpp>
#include <random>

namespace crv::division {
namespace {

template <typename t_dividend_t, typename t_divisor_t> struct test_case_t
{
    using dividend_t = t_dividend_t;
    using divisor_t  = t_divisor_t;
};

template <typename test_types_t> struct division_long_divider_test_t : ::testing::Test
{
    using dividend_t = test_types_t::dividend_t;
    using divisor_t  = test_types_t::divisor_t;
    using result_t   = result_t<dividend_t, divisor_t>;

    static constexpr auto dividend_bit_count = sizeof(dividend_t) * CHAR_BIT;
    static constexpr auto max_dividend       = ~dividend_t{0};
    static constexpr auto max_divisor        = ~divisor_t{0};
    static constexpr auto shift              = dividend_bit_count / 2;
    static constexpr auto low_mask           = (dividend_t{1} << shift) - 1;

    struct instruction_t
    {
        auto verify_preconditions(dividend_t dividend, divisor_t divisor) const noexcept -> void
        {
            ASSERT_LT(dividend >> shift, divisor);
        }

        auto operator()(dividend_t dividend, divisor_t divisor) const noexcept -> result_t
        {
            verify_preconditions(dividend, divisor);

            return result_t{.quotient  = int_cast<dividend_t>(dividend / divisor),
                            .remainder = int_cast<divisor_t>(dividend % divisor)};
        }
    };

    using sut_t = long_divider_t<dividend_t, divisor_t, instruction_t>;

    sut_t sut{};

    auto test(dividend_t dividend, divisor_t divisor) const noexcept -> result_t
    {
        return sut(dividend, divisor, instruction_t{});
    }

    struct full_width_generator_t
    {
        auto dividend(auto& rng) noexcept -> dividend_t
        {
            return (static_cast<dividend_t>(rng()) << shift) | static_cast<dividend_t>(rng());
        }

        auto divisor(auto& rng) noexcept -> dividend_t { return static_cast<divisor_t>(rng()); }
    };

    struct random_length_generator_t : full_width_generator_t
    {
        std::uniform_int_distribution<int_t> bit_count_dist{0, dividend_bit_count - 1};

        template <typename value_t> auto randomize_length(auto& rng, value_t value) noexcept -> value_t
        {
            return value >> bit_count_dist(rng);
        }

        auto dividend(auto& rng) noexcept -> dividend_t
        {
            return randomize_length(rng, full_width_generator_t::dividend(rng));
        }

        auto divisor(auto& rng) noexcept -> dividend_t
        {
            return randomize_length(rng, full_width_generator_t::divisor(rng));
        }
    };

    auto fuzz(auto&& generate) const noexcept -> void
    {
        auto rng = std::mt19937_64{0xF12345678};

        for (auto i = 0; i < 10000; ++i)
        {
            auto const dividend = generate.dividend(rng);

            // ensure divisor is never 0
            auto const divisor = generate.divisor(rng) | 1;

            auto const expected
                = result_t{.quotient = dividend / divisor, .remainder = int_cast<divisor_t>(dividend % divisor)};

            auto const actual = this->test(dividend, divisor);

            EXPECT_EQ(expected, actual);
        }
    }
};

using test_types_t = ::testing::Types<
    // N/N
    test_case_t<uint32_t, uint32_t>, // N/N
    test_case_t<uint64_t, uint64_t>, // N/N

    // 2N/N
    test_case_t<uint64_t, uint32_t>, // 2N/N
    test_case_t<uint128_t, uint64_t> // 2N/N
    >;
TYPED_TEST_SUITE(division_long_divider_test_t, test_types_t);

TYPED_TEST(division_long_divider_test_t, zero)
{
    using dividend_t = TestFixture::dividend_t;
    using divisor_t  = TestFixture::divisor_t;
    using result_t   = TestFixture::result_t;

    auto const dividend = dividend_t{0};
    auto const divisor  = divisor_t{1};
    auto const expected = result_t{.quotient = 0, .remainder = 0};

    auto const actual = this->test(dividend, divisor);

    EXPECT_EQ(expected, actual);
}

TYPED_TEST(division_long_divider_test_t, identity)
{
    using dividend_t = TestFixture::dividend_t;
    using divisor_t  = TestFixture::divisor_t;
    using result_t   = TestFixture::result_t;

    auto const dividend = dividend_t{1};
    auto const divisor  = divisor_t{1};
    auto const expected = result_t{.quotient = 1, .remainder = 0};

    auto const actual = this->test(dividend, divisor);

    EXPECT_EQ(expected, actual);
}

TYPED_TEST(division_long_divider_test_t, self_division)
{
    using dividend_t = TestFixture::dividend_t;
    using divisor_t  = TestFixture::divisor_t;
    using result_t   = TestFixture::result_t;

    auto const dividend = dividend_t{5000}; // arbitrary, > 1, fits in divisor_t
    auto const divisor  = int_cast<divisor_t>(dividend);
    auto const expected = result_t{.quotient = 1, .remainder = 0};

    auto const actual = this->test(dividend, divisor);

    EXPECT_EQ(expected, actual);
}

TYPED_TEST(division_long_divider_test_t, basic)
{
    using dividend_t = TestFixture::dividend_t;
    using divisor_t  = TestFixture::divisor_t;
    using result_t   = TestFixture::result_t;

    auto const dividend = dividend_t{6};
    auto const divisor  = divisor_t{3};
    auto const expected = result_t{.quotient = 2, .remainder = 0};

    auto const actual = this->test(dividend, divisor);

    EXPECT_EQ(expected, actual);
}

/*
    tests highest possible dividend for an arbitrary divisor where first instruction's quotient is zero

        [n - 1][~0]/n

    This maximizes the remainder carried to the second step. This is the last value for the given divisor that does not
    trap.
*/
TYPED_TEST(division_long_divider_test_t, test_high_divisor_minus_one)
{
    using dividend_t = TestFixture::dividend_t;
    using divisor_t  = TestFixture::divisor_t;
    using result_t   = TestFixture::result_t;

    auto const divisor  = divisor_t{5000};       // arbitrary
    auto const high     = int_cast<dividend_t>(divisor - 1);
    auto const low      = TestFixture::low_mask; // maximize low half
    auto const dividend = (high << TestFixture::shift) | low;
    auto const expected = result_t{.quotient = low, .remainder = int_cast<divisor_t>(high)};

    auto const actual = this->test(dividend, divisor);

    EXPECT_EQ(expected, actual);
}

/*
    tests lowest possible dividend for an arbitrary divisor where the first instruction's quotient is nonzero

        [n][0]/n

    This is the first value for the given divisor that traps.
*/
TYPED_TEST(division_long_divider_test_t, high_equals_divisor)
{
    using dividend_t = TestFixture::dividend_t;
    using divisor_t  = TestFixture::divisor_t;
    using result_t   = TestFixture::result_t;

    auto const divisor  = divisor_t{5000}; // arbitrary
    auto const high     = dividend_t{divisor};
    auto const low      = dividend_t{TestFixture::low_mask};
    auto const dividend = (high << TestFixture::shift) | low;
    auto const expected
        = result_t{.quotient = dividend / divisor, .remainder = int_cast<divisor_t>(dividend % divisor)};

    auto const actual = this->test(dividend, divisor);

    EXPECT_EQ(expected, actual);
}

/*
    tests maximum remainder that can be carried into the low half

        [~0 - 1][~0]/~0

    This maximizes both the shifted remainder and the low half to stress the bitwise or.
*/
TYPED_TEST(division_long_divider_test_t, max_divisor_max_remainder_carry)
{
    using dividend_t = TestFixture::dividend_t;
    using divisor_t  = TestFixture::divisor_t;
    using result_t   = TestFixture::result_t;

    auto const divisor  = TestFixture::max_divisor;
    auto const high     = int_cast<dividend_t>(divisor - 1);
    auto const low      = TestFixture::low_mask;
    auto const dividend = (high << TestFixture::shift) | low;
    auto const expected
        = result_t{.quotient = dividend / divisor, .remainder = int_cast<divisor_t>(dividend % divisor)};

    auto const actual = this->test(dividend, divisor);

    EXPECT_EQ(expected, actual);
}

/*
    tests condition where remainder carry is large, but low bits are empty

        [~0 - 1][0]/~0

    This ensures the shift and bitwise or don't rely on low bits being present.
*/
TYPED_TEST(division_long_divider_test_t, empty_low_half)
{
    using dividend_t = TestFixture::dividend_t;
    using divisor_t  = TestFixture::divisor_t;
    using result_t   = TestFixture::result_t;

    auto const divisor  = int_cast<divisor_t>(TestFixture::max_divisor);
    auto const high     = int_cast<dividend_t>(divisor - 1);
    auto const low      = int_cast<dividend_t>(0);
    auto const dividend = (high << TestFixture::shift) | low;
    auto const expected
        = result_t{.quotient = dividend / divisor, .remainder = int_cast<divisor_t>(dividend % divisor)};

    auto const actual = this->test(dividend, divisor);

    EXPECT_EQ(expected, actual);
}

/*
    tests condition where the entire dividend is smaller than the divisor.

        [0][~0 - 1]/~0

    High half is 0, quotient should be 0, and remainder should be the dividend.
*/
TYPED_TEST(division_long_divider_test_t, dividend_smaller_than_divisor)
{
    using dividend_t = TestFixture::dividend_t;
    using divisor_t  = TestFixture::divisor_t;
    using result_t   = TestFixture::result_t;

    auto const divisor  = TestFixture::max_divisor;
    auto const dividend = int_cast<dividend_t>(divisor - 1);
    auto const expected = result_t{.quotient = 0, .remainder = int_cast<divisor_t>(dividend)};

    auto const actual = this->test(dividend, divisor);

    EXPECT_EQ(actual.quotient, 0);
    EXPECT_EQ(actual.remainder, dividend);
    EXPECT_EQ(expected, actual);
}

TYPED_TEST(division_long_divider_test_t, only_msb_set)
{
    using dividend_t = TestFixture::dividend_t;
    using divisor_t  = TestFixture::divisor_t;
    using result_t   = TestFixture::result_t;

    // set highest bit of dividend
    auto const dividend = dividend_t{1} << (TestFixture::dividend_bit_count - 1);

    // use small prime divisor to ensure both quotient and remainder are generated
    auto const divisor = divisor_t{3};

    auto const expected = result_t{.quotient = dividend / divisor, .remainder = 2};

    auto const actual = this->test(dividend, divisor);

    EXPECT_EQ(expected, actual);
}

/*
    tests maximum possible double-width dividend divided by 1

        [~0][~0]1

    Tests that no bits are dropped or overflowed during reassembly. This is the largest result possible.
*/
TYPED_TEST(division_long_divider_test_t, max_capacity)
{
    using divisor_t = TestFixture::divisor_t;
    using result_t  = TestFixture::result_t;

    auto const dividend = TestFixture::max_dividend;
    auto const divisor  = divisor_t{1};
    auto const expected = result_t{.quotient = dividend, .remainder = 0};

    auto const actual = this->test(dividend, divisor);

    EXPECT_EQ(expected, actual);
}

/*
    tests edge case where divisor is exactly a power of two

    This tests off-by-one errors in bit-shifting logic.
*/
TYPED_TEST(division_long_divider_test_t, power_of_two_divisor)
{
    using divisor_t = TestFixture::divisor_t;
    using result_t  = TestFixture::result_t;

    auto const divisor  = divisor_t{1} << (TestFixture::shift - 1);
    auto const dividend = TestFixture::max_dividend;
    auto const expected = result_t{.quotient = dividend / divisor, .remainder = divisor - 1};

    auto const actual = this->test(dividend, divisor);

    EXPECT_EQ(expected, actual);
}

/*
    tests case where high is exact multiple of divisor, so first remainder is 0
*/
TYPED_TEST(division_long_divider_test_t, zero_remainder_carry)
{
    using dividend_t = TestFixture::dividend_t;
    using divisor_t  = TestFixture::divisor_t;
    using result_t   = TestFixture::result_t;

    auto const divisor  = divisor_t{256};
    auto const high     = int_cast<dividend_t>(divisor * 3);
    auto const low      = int_cast<dividend_t>(divisor + 1);
    auto const dividend = (high << TestFixture::shift) | low;
    auto const expected
        = result_t{.quotient = dividend / divisor, .remainder = int_cast<divisor_t>(dividend % divisor)};

    auto const actual = this->test(dividend, divisor);

    EXPECT_EQ(expected, actual);
}

TYPED_TEST(division_long_divider_test_t, alternating_bits_even)
{
    using divisor_t = TestFixture::divisor_t;
    using result_t  = TestFixture::result_t;

    // 0xAAAA...AAAA
    auto const dividend = TestFixture::max_dividend / 3 * 2;
    auto const divisor  = divisor_t{7};
    auto const expected
        = result_t{.quotient = dividend / divisor, .remainder = int_cast<divisor_t>(dividend % divisor)};

    auto const actual = this->test(dividend, divisor);

    EXPECT_EQ(expected, actual);
}

TYPED_TEST(division_long_divider_test_t, alternating_bits_odd)
{
    using divisor_t = TestFixture::divisor_t;
    using result_t  = TestFixture::result_t;

    // 0x5555...5555
    auto const dividend = TestFixture::max_dividend / 3;
    auto const divisor  = divisor_t{7};
    auto const expected
        = result_t{.quotient = dividend / divisor, .remainder = int_cast<divisor_t>(dividend % divisor)};

    auto const actual = this->test(dividend, divisor);

    EXPECT_EQ(expected, actual);
}

TYPED_TEST(division_long_divider_test_t, fuzz_full_width)
{
    TestFixture::fuzz(typename TestFixture::full_width_generator_t{});
}

TYPED_TEST(division_long_divider_test_t, fuzz_random_length)
{
    TestFixture::fuzz(typename TestFixture::random_length_generator_t{});
}

} // namespace
} // namespace crv::division
