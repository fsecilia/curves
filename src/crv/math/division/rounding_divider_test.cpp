// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "rounding_divider.hpp"
#include <crv/math/division/result.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::division {
namespace {

struct division_rounding_mode_test_t : Test
{
    using narrow_t = uint8_t;
    using wide_t   = wider_t<narrow_t>;
    using result_t = result_t<wide_t, narrow_t>;

    struct mock_rounding_mode_t
    {
        MOCK_METHOD(wide_t, div_bias, (wide_t dividend, narrow_t divisor), (const, noexcept));
        MOCK_METHOD(wide_t, div_carry, (wide_t quotient, narrow_t divisor, narrow_t remainder), (const, noexcept));
        virtual ~mock_rounding_mode_t() = default;
    };
    StrictMock<mock_rounding_mode_t> mock_rounding_mode{};

    struct rounding_mode_t
    {
        mock_rounding_mode_t* mock = nullptr;

        auto div_bias(wide_t dividend, narrow_t divisor) const noexcept -> wide_t
        {
            return mock->div_bias(dividend, divisor);
        }

        auto div_carry(wide_t quotient, narrow_t divisor, narrow_t remainder) const noexcept -> wide_t
        {
            return mock->div_carry(quotient, divisor, remainder);
        }
    };
    rounding_mode_t rounding_mode{&mock_rounding_mode};

    struct mock_divider_t
    {
        MOCK_METHOD(result_t, divide, (wide_t dividend, narrow_t divisor), (const, noexcept));
        virtual ~mock_divider_t() = default;
    };
    StrictMock<mock_divider_t> mock_divider;

    struct divider_t
    {
        mock_divider_t* mock = nullptr;

        auto operator()(wide_t dividend, narrow_t divisor) const noexcept -> result_t
        {
            return mock->divide(dividend, divisor);
        }
    };

    using sut_t = rounding_divider_t<narrow_t, divider_t>;
    sut_t const sut{divider_t{&mock_divider}};
};

TEST_F(division_rounding_mode_test_t, divide)
{
    auto const seq = InSequence{};

    // all arbitrary
    auto const dividend          = wide_t{2593};
    auto const divisor           = narrow_t{59};
    auto const biased_dividend   = dividend + 1;
    auto const result            = result_t{443, 3};
    auto const expected_quotient = wide_t{541};

    EXPECT_CALL(mock_rounding_mode, div_bias(dividend, divisor)).WillOnce(Return(biased_dividend));
    EXPECT_CALL(mock_divider, divide(biased_dividend, divisor)).WillOnce(Return(result));
    EXPECT_CALL(mock_rounding_mode, div_carry(result.quotient, divisor, result.remainder))
        .WillOnce(Return(expected_quotient));

    auto const actual_quotient = sut(dividend, divisor, rounding_mode);

    EXPECT_EQ(expected_quotient, actual_quotient);
}

} // namespace
} // namespace crv::division
