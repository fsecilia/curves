// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "dispatching_divider.hpp"
#include <crv/math/io.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>
#include <ostream>

namespace crv::division {
namespace {

struct param_t
{
    using wide_t   = uint16_t;
    using narrow_t = uint8_t;

    wide_t   dividend;
    narrow_t divisor;
    bool     dispatches_to_hardware;

    friend auto operator<<(std::ostream& dst, param_t const& src) -> std::ostream&
    {
        return dst << "{.dividend = " << static_cast<int_t>(src.dividend)
                   << ", .divisor = " << static_cast<int_t>(src.divisor)
                   << ", .dispatches_to_hardware = " << std::boolalpha << src.dispatches_to_hardware << "}";
    }
};

struct division_dispatching_divider_test_t : TestWithParam<param_t>
{
    using wide_t   = param_t::wide_t;
    using narrow_t = param_t::narrow_t;

    using narrow_result_t = result_t<narrow_t>;
    using wide_result_t   = result_t<wide_t, narrow_t>;

    wide_t const   dividend               = GetParam().dividend;
    narrow_t const divisor                = GetParam().divisor;
    bool const     dispatches_to_hardware = GetParam().dispatches_to_hardware;

    struct mock_hardware_divider_t
    {
        MOCK_METHOD(narrow_result_t, divide, (wide_t dividend, narrow_t divisor), (const, noexcept));

        virtual ~mock_hardware_divider_t() = default;
    };
    StrictMock<mock_hardware_divider_t> mock_hardware_divider;

    struct hardware_divider_t
    {
        mock_hardware_divider_t* mock = nullptr;

        auto operator()(wide_t dividend, narrow_t divisor) const noexcept -> narrow_result_t
        {
            return mock->divide(dividend, divisor);
        }
    };

    struct mock_long_divider_t
    {
        MOCK_METHOD(wide_result_t, divide, (wide_t dividend, narrow_t divisor), (const, noexcept));

        virtual ~mock_long_divider_t() = default;
    };
    StrictMock<mock_long_divider_t> mock_long_divider;

    struct long_divider_t
    {
        mock_long_divider_t* mock = nullptr;

        auto operator()(wide_t dividend, narrow_t divisor) const noexcept -> wide_result_t
        {
            return mock->divide(dividend, divisor);
        }
    };

    narrow_result_t const expected_hardware_result{17, 31};
    wide_result_t const   expected_long_result{19, 37};

    using sut_t = dispatching_divider_t<wide_t, narrow_t, hardware_divider_t, long_divider_t>;
    sut_t sut{hardware_divider_t{&mock_hardware_divider}, long_divider_t{&mock_long_divider}};
};

TEST_P(division_dispatching_divider_test_t, result)
{
    wide_result_t expected;
    if (dispatches_to_hardware)
    {
        expected.quotient  = static_cast<wide_t>(expected_hardware_result.quotient);
        expected.remainder = expected_hardware_result.remainder;
        EXPECT_CALL(mock_hardware_divider, divide(dividend, divisor)).WillOnce(Return(expected_hardware_result));
    }
    else
    {
        expected = expected_long_result;
        EXPECT_CALL(mock_long_divider, divide(dividend, divisor)).WillOnce(Return(expected_long_result));
    }

    auto const actual = sut(dividend, divisor);

    EXPECT_EQ(expected, actual);
}

// tests exact h - 1, h, and h + 1 boundaries of the high half of the dividend.
param_t const test_vectors[] = {
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
INSTANTIATE_TEST_SUITE_P(cases, division_dispatching_divider_test_t, ValuesIn(test_vectors));

} // namespace
} // namespace crv::division
