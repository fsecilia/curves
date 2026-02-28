// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include "dispatcher.hpp"
#include <crv/math/io.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>
#include <ostream>

namespace crv::division {
namespace {

using single_width_t        = uint8_t;
using double_width_t        = uint16_t;
using single_width_result_t = result_t<single_width_t>;
using double_width_result_t = result_t<double_width_t, single_width_t>;

// --------------------------------------------------------------------------------------------------------------------
// generic dispatcher
// --------------------------------------------------------------------------------------------------------------------

struct division_dispatcher_test_generic_t : Test
{
    using dividend_t = single_width_t;
    using divisor_t  = single_width_t;
    using result_t   = single_width_result_t;

    struct mock_divider_t
    {
        MOCK_METHOD(result_t, divide, (dividend_t dividend, divisor_t divisor), (const, noexcept));

        virtual ~mock_divider_t() = default;
    };
    StrictMock<mock_divider_t> mock_hardware_divider;
    StrictMock<mock_divider_t> mock_long_divider;

    struct divider_t
    {
        mock_divider_t* mock = nullptr;

        auto operator()(dividend_t dividend, divisor_t divisor) const noexcept -> result_t
        {
            return mock->divide(dividend, divisor);
        }
    };
    divider_t hardware_divider{&mock_hardware_divider};
    divider_t long_divider{&mock_long_divider};

    using sut_t = dispatcher_t<single_width_t, single_width_t, divider_t, divider_t>;
    sut_t sut{};
};

TEST_F(division_dispatcher_test_generic_t, executes_hardware_passthrough)
{
    auto const dividend = dividend_t{0xFF};
    auto const divisor  = divisor_t{0x02};

    auto const expected = result_t{dividend, divisor};
    EXPECT_CALL(mock_hardware_divider, divide(dividend, divisor)).WillOnce(Return(expected));

    auto const actual = sut(dividend, divisor, hardware_divider, long_divider);

    EXPECT_EQ(expected, actual);
}

// --------------------------------------------------------------------------------------------------------------------
// double width dispatcher
// --------------------------------------------------------------------------------------------------------------------

struct double_width_param_t
{
    using dividend_t = double_width_t;
    using divisor_t  = single_width_t;

    dividend_t dividend;
    divisor_t  divisor;
    bool       dispatches_to_hardware;

    friend auto operator<<(std::ostream& dst, double_width_param_t const& src) -> std::ostream&
    {
        return dst << "{.dividend = " << static_cast<int_t>(src.dividend)
                   << ", .divisor = " << static_cast<int_t>(src.divisor)
                   << ", .dispatches_to_hardware = " << std::boolalpha << src.dispatches_to_hardware << "}";
    }
};

struct division_dispatcher_test_double_width_t : Test, WithParamInterface<double_width_param_t>
{
    using dividend_t = ParamType::dividend_t;
    using divisor_t  = ParamType::divisor_t;

    dividend_t const dividend               = GetParam().dividend;
    divisor_t const  divisor                = GetParam().divisor;
    bool const       dispatches_to_hardware = GetParam().dispatches_to_hardware;

    struct mock_hardware_divider_t
    {
        MOCK_METHOD(single_width_result_t, divide, (dividend_t dividend, divisor_t divisor), (const, noexcept));

        virtual ~mock_hardware_divider_t() = default;
    };
    StrictMock<mock_hardware_divider_t> mock_hardware_divider;

    struct hardware_divider_t
    {
        mock_hardware_divider_t* mock = nullptr;

        auto operator()(dividend_t dividend, divisor_t divisor) const noexcept -> single_width_result_t
        {
            return mock->divide(dividend, divisor);
        }
    };
    hardware_divider_t hardware_divider{&mock_hardware_divider};

    struct mock_long_divider_t
    {
        MOCK_METHOD(double_width_result_t, divide, (dividend_t dividend, divisor_t divisor), (const, noexcept));

        virtual ~mock_long_divider_t() = default;
    };
    StrictMock<mock_long_divider_t> mock_long_divider;

    struct long_divider_t
    {
        mock_long_divider_t* mock = nullptr;

        auto operator()(dividend_t dividend, divisor_t divisor) const noexcept -> double_width_result_t
        {
            return mock->divide(dividend, divisor);
        }
    };
    long_divider_t long_divider{&mock_long_divider};

    single_width_result_t const expected_hardware_result{17, 31};
    double_width_result_t const expected_long_result{19, 37};

    using sut_t = dispatcher_t<double_width_t, single_width_t, hardware_divider_t, long_divider_t>;
    sut_t sut{};
};

TEST_P(division_dispatcher_test_double_width_t, result)
{
    double_width_result_t expected;
    if (dispatches_to_hardware)
    {
        expected.quotient  = static_cast<double_width_t>(expected_hardware_result.quotient);
        expected.remainder = expected_hardware_result.remainder;
        EXPECT_CALL(mock_hardware_divider, divide(dividend, divisor)).WillOnce(Return(expected_hardware_result));
    }
    else
    {
        expected = expected_long_result;
        EXPECT_CALL(mock_long_divider, divide(dividend, divisor)).WillOnce(Return(expected_long_result));
    }

    auto const actual = sut(dividend, divisor, hardware_divider, long_divider);

    EXPECT_EQ(expected, actual);
}

// tests exact h - 1, h, and h + 1 boundaries of the high half of the dividend.
double_width_param_t const double_width_test_vectors[] = {
    // minimum high-half
    {0x00'00, 0x01, true},
    {0x00'FF, 0x01, true},

    // typical low high-half
    {0x01'FF, 0x01, false},
    {0x01'FF, 0x02, true},

    // midpoint high-half
    {0x7F'FF, 0x7E, false},
    {0x7F'FF, 0x7F, false},
    {0x7F'FF, 0x80, true},

    // maximum high-half minus one
    {0xFE'FF, 0xFD, false},
    {0xFE'FF, 0xFE, false},
    {0xFE'FF, 0xFF, true},

    // absolute maximum high-half
    {0xFF'FF, 0xFE, false},
    {0xFF'FF, 0xFF, false},
};
INSTANTIATE_TEST_SUITE_P(cases, division_dispatcher_test_double_width_t, ValuesIn(double_width_test_vectors));

} // namespace
} // namespace crv::division
