// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "normalizing_dividers.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <compare>
#include <gmock/gmock.h>

namespace crv::division::normalizing_dividers {
namespace {

struct division_normalizing_dividers_test_t : Test
{
    using narrow_t = uint8_t;
    using wide_t   = uint16_t;
    using result_t = result_t<wide_t, narrow_t>;

    static constexpr auto mock_divisor          = narrow_t{7};
    static constexpr auto mock_remainder        = narrow_t{5};
    static constexpr auto mock_shifted_quotient = wide_t{3};
    static constexpr auto expected_div_shr      = wide_t{11};
    static constexpr auto expected_div_bias     = wide_t{13};
    static constexpr auto expected_div_carry    = wide_t{17};

    struct mock_divider_t
    {
        MOCK_METHOD(result_t, divide, (wide_t dividend, narrow_t divisor), (const, noexcept));
        virtual ~mock_divider_t() = default;
    };
    mock_divider_t mock_divider;

    struct divider_t
    {
        mock_divider_t* mock;

        auto operator()(wide_t dividend, narrow_t divisor) const noexcept -> result_t
        {
            return mock->divide(dividend, divisor);
        }
    };

    struct mock_rounding_mode_t
    {
        MOCK_METHOD(wide_t, div_bias, (wide_t dividend, narrow_t divisor), (const, noexcept));
        MOCK_METHOD(wide_t, div_carry, (wide_t quotient, narrow_t divisor, narrow_t remainder), (const, noexcept));
        MOCK_METHOD(wide_t, div_shr,
                    (wide_t shifted_quotient, wide_t quotient, narrow_t divisor, narrow_t remainder, int_t shift),
                    (const, noexcept));
        virtual ~mock_rounding_mode_t() = default;
    };
    mock_rounding_mode_t mock_rounding_mode;

    struct rounding_mode_t
    {
        mock_rounding_mode_t* mock;

        auto div_bias(wide_t dividend, narrow_t divisor) const noexcept -> wide_t
        {
            return mock->div_bias(dividend, divisor);
        }

        auto div_carry(wide_t quotient, narrow_t divisor, narrow_t remainder) const noexcept -> wide_t
        {
            return mock->div_carry(quotient, divisor, remainder);
        }

        auto div_shr(wide_t shifted_quotient, wide_t quotient, narrow_t divisor, narrow_t remainder,
                     int_t shift) const noexcept -> wide_t
        {
            return mock->div_shr(shifted_quotient, quotient, divisor, remainder, shift);
        }
    };
    rounding_mode_t rounding_mode{&mock_rounding_mode};
};

// --------------------------------------------------------------------------------------------------------------------
// variable_t
// --------------------------------------------------------------------------------------------------------------------

struct division_normalizing_dividers_test_variable_t : division_normalizing_dividers_test_t
{
    template <int total_shift> using sut_t = variable_t<narrow_t, total_shift, divider_t>;

    template <int total_shift> auto make_sut() -> sut_t<total_shift>
    {
        return sut_t<total_shift>{divider_t{&mock_divider}};
    }
};

// variable_t always routes through div_shr
TEST_F(division_normalizing_dividers_test_variable_t, uses_div_shr)
{
    constexpr auto total_shift = 12;
    constexpr auto dividend    = wide_t{11};
    constexpr auto clz         = std::countl_zero(dividend);
    constexpr auto post_shift  = clz - total_shift;
    constexpr auto quotient    = mock_shifted_quotient << post_shift;

    EXPECT_CALL(mock_divider, divide(dividend << clz, mock_divisor))
        .WillOnce(Return(result_t{quotient, mock_remainder}));
    EXPECT_CALL(mock_rounding_mode, div_shr(mock_shifted_quotient, quotient, mock_divisor, mock_remainder, post_shift))
        .WillOnce(Return(expected_div_shr));

    auto const actual = make_sut<total_shift>()(dividend, mock_divisor, rounding_mode);

    EXPECT_EQ(expected_div_shr, actual);
}

// tests |1 trick
TEST_F(division_normalizing_dividers_test_variable_t, handles_zero_dividend_safely)
{
    constexpr auto total_shift = 15;
    constexpr auto dividend    = wide_t{0};
    constexpr auto clz         = std::countl_zero(static_cast<wide_t>(dividend | 1));
    constexpr auto post_shift  = clz - total_shift;
    constexpr auto quotient    = mock_shifted_quotient << post_shift;

    EXPECT_CALL(mock_divider, divide(dividend << clz, mock_divisor))
        .WillOnce(Return(result_t{quotient, mock_remainder}));
    EXPECT_CALL(mock_rounding_mode, div_shr(_, quotient, _, _, post_shift)).WillOnce(Return(expected_div_shr));

    make_sut<total_shift>()(dividend, mock_divisor, rounding_mode);
}

// max narrow_t (0xFF) has 8 leading zeros in a 16-bit wide_t
TEST_F(division_normalizing_dividers_test_variable_t, handles_minimum_headroom)
{
    constexpr auto total_shift = 8;
    constexpr auto dividend    = wide_t{0xFF};
    constexpr auto clz         = 8;
    constexpr auto post_shift  = 0; // boundary: post_shift bottoms out at 0
    constexpr auto quotient    = mock_shifted_quotient << post_shift;

    EXPECT_CALL(mock_divider, divide(dividend << clz, mock_divisor))
        .WillOnce(Return(result_t{quotient, mock_remainder}));
    EXPECT_CALL(mock_rounding_mode, div_shr(_, _, _, _, post_shift)).WillOnce(Return(expected_div_shr));

    make_sut<total_shift>()(dividend, mock_divisor, rounding_mode);
}

TEST_F(division_normalizing_dividers_test_variable_t, handles_zero_shift)
{
    constexpr auto total_shift = 0;
    constexpr auto dividend    = wide_t{11};
    constexpr auto clz         = 12;
    constexpr auto post_shift  = clz;

    EXPECT_CALL(mock_divider, divide(_, _))
        .WillOnce(Return(result_t{mock_shifted_quotient << post_shift, mock_remainder}));
    EXPECT_CALL(mock_rounding_mode, div_shr(_, _, _, _, post_shift)).WillOnce(Return(expected_div_shr));

    make_sut<total_shift>()(dividend, mock_divisor, rounding_mode);
}

// tests that shifting a value that only has 8 leading zeros by 9 triggers the assert
TEST_F(division_normalizing_dividers_test_variable_t, asserts_on_insufficient_headroom)
{
    constexpr auto total_shift = 9;
    constexpr auto dividend    = wide_t{0xFF};

    EXPECT_DEBUG_DEATH(make_sut<total_shift>()(dividend, mock_divisor, rounding_mode),
                       "left shift after divide not supported");
}

// --------------------------------------------------------------------------------------------------------------------
// constant_t tests
// --------------------------------------------------------------------------------------------------------------------

struct division_normalizing_dividers_test_constant_t : division_normalizing_dividers_test_t
{
    template <narrow_t constant_dividend, int total_shift>
    using sut_t = constant_t<narrow_t, constant_dividend, total_shift, divider_t>;

    template <narrow_t constant_dividend, int total_shift> auto make_sut() -> sut_t<constant_dividend, total_shift>
    {
        return sut_t<constant_dividend, total_shift>{divider_t{&mock_divider}};
    }
};

TEST_F(division_normalizing_dividers_test_constant_t, fast_path_min)
{
    constexpr auto constant_dividend = narrow_t{11};
    constexpr auto total_shift       = 0;
    constexpr auto pre_shift         = total_shift;

    EXPECT_CALL(mock_rounding_mode, div_bias(wide_t{constant_dividend} << pre_shift, mock_divisor))
        .WillOnce(Return(expected_div_bias));
    EXPECT_CALL(mock_divider, divide(expected_div_bias, mock_divisor))
        .WillOnce(Return(result_t{mock_shifted_quotient, mock_remainder}));
    EXPECT_CALL(mock_rounding_mode, div_carry(mock_shifted_quotient, mock_divisor, mock_remainder))
        .WillOnce(Return(expected_div_carry));

    auto const actual = make_sut<constant_dividend, total_shift>()(constant_dividend, mock_divisor, rounding_mode);

    EXPECT_EQ(expected_div_carry, actual);
}

TEST_F(division_normalizing_dividers_test_constant_t, fast_path_max)
{
    constexpr auto constant_dividend = narrow_t{11};
    constexpr auto total_shift       = 11;
    constexpr auto pre_shift         = total_shift;

    EXPECT_CALL(mock_rounding_mode, div_bias(wide_t{constant_dividend} << pre_shift, mock_divisor))
        .WillOnce(Return(expected_div_bias));
    EXPECT_CALL(mock_divider, divide(expected_div_bias, mock_divisor))
        .WillOnce(Return(result_t{mock_shifted_quotient, mock_remainder}));
    EXPECT_CALL(mock_rounding_mode, div_carry(mock_shifted_quotient, mock_divisor, mock_remainder))
        .WillOnce(Return(expected_div_carry));

    auto const actual = make_sut<constant_dividend, total_shift>()(constant_dividend, mock_divisor, rounding_mode);

    EXPECT_EQ(expected_div_carry, actual);
}

TEST_F(division_normalizing_dividers_test_constant_t, slow_path_min)
{
    constexpr auto constant_dividend = narrow_t{11};
    constexpr auto total_shift       = 12;
    constexpr auto pre_shift         = 12;
    constexpr auto post_shift        = pre_shift - total_shift;
    constexpr auto quotient          = mock_shifted_quotient << -post_shift;

    EXPECT_CALL(mock_divider, divide(wide_t{constant_dividend} << pre_shift, mock_divisor))
        .WillOnce(Return(result_t{quotient, mock_remainder}));
    EXPECT_CALL(mock_rounding_mode, div_shr(mock_shifted_quotient, quotient, mock_divisor, mock_remainder, -post_shift))
        .WillOnce(Return(expected_div_shr));

    auto const actual = make_sut<constant_dividend, total_shift>()(constant_dividend, mock_divisor, rounding_mode);

    EXPECT_EQ(expected_div_shr, actual);
}

TEST_F(division_normalizing_dividers_test_constant_t, slow_path_max)
{
    constexpr auto constant_dividend = narrow_t{11};
    constexpr auto total_shift       = 15;
    constexpr auto pre_shift         = 12;
    constexpr auto post_shift        = pre_shift - total_shift;
    constexpr auto quotient          = mock_shifted_quotient << -post_shift;

    EXPECT_CALL(mock_divider, divide(wide_t{constant_dividend} << pre_shift, mock_divisor))
        .WillOnce(Return(result_t{quotient, mock_remainder}));
    EXPECT_CALL(mock_rounding_mode, div_shr(mock_shifted_quotient, quotient, mock_divisor, mock_remainder, -post_shift))
        .WillOnce(Return(expected_div_shr));

    auto const actual = make_sut<constant_dividend, total_shift>()(constant_dividend, mock_divisor, rounding_mode);

    EXPECT_EQ(expected_div_shr, actual);
}

// caps headroom at 15 because of `|1`
TEST_F(division_normalizing_dividers_test_constant_t, handles_zero_dividend_safely)
{
    constexpr auto constant_dividend = narrow_t{0};
    constexpr auto total_shift       = 10;

    EXPECT_CALL(mock_rounding_mode, div_bias(_, _)).WillOnce(Return(expected_div_bias));
    EXPECT_CALL(mock_divider, divide(_, _)).WillOnce(Return(result_t{mock_shifted_quotient, mock_remainder}));
    EXPECT_CALL(mock_rounding_mode, div_carry(_, _, _)).WillOnce(Return(expected_div_carry));

    make_sut<constant_dividend, total_shift>()(constant_dividend, mock_divisor, rounding_mode);
}

TEST_F(division_normalizing_dividers_test_constant_t, mismatched_runtime_dividend)
{
    constexpr auto constant_dividend = narrow_t{11};
    constexpr auto total_shift       = 10;
    constexpr auto wrong_dividend    = narrow_t{99};

    EXPECT_DEBUG_DEATH((make_sut<constant_dividend, total_shift>()(wrong_dividend, mock_divisor, rounding_mode)),
                       "runtime dividend must match specified constant");
}

} // namespace
} // namespace crv::division::normalizing_dividers
