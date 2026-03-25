// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "wide_divider.hpp"
#include <crv/math/io.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>
#include <random>

namespace crv::division {
namespace {

template <typename narrow_t> struct fake_hardware_divider_t
{
    using wide_t   = wider_t<narrow_t>;
    using result_t = qr_pair_t<narrow_t>;

    constexpr auto operator()(wide_t dividend, narrow_t divisor) const noexcept -> result_t
    {
        return {int_cast<narrow_t>(dividend / divisor), int_cast<narrow_t>(dividend % divisor)};
    }
};

template <typename narrow_t> struct stub_rounding_mode_t
{
    using wide_t = wider_t<narrow_t>;
    constexpr auto bias(wide_t dividend, narrow_t) const noexcept -> wide_t { return dividend; }
    constexpr auto carry(wide_t quotient, narrow_t, narrow_t) const noexcept -> wide_t { return quotient; }
};

// ====================================================================================================================
// Rounding Contract
//
// Tests bias, divide, and carry are executed in order on both fast and slow paths.
// ====================================================================================================================

namespace rounding_contract {

using narrow_t = uint8_t;
using wide_t   = wider_t<narrow_t>;

using hardware_divider_t = fake_hardware_divider_t<narrow_t>;
using result_t           = hardware_divider_t::result_t;

template <typename narrow_t> struct fake_rounding_mode_t
{
    using wide_t = wider_t<narrow_t>;

    constexpr auto bias(wide_t dividend, narrow_t divisor) const noexcept -> wide_t { return dividend * 2 + divisor; }

    constexpr auto carry(wide_t quotient, narrow_t divisor, narrow_t remainder) const noexcept -> wide_t
    {
        return quotient * 5 + divisor * 7 + remainder * 11;
    }
};
constexpr auto rounding_mode = fake_rounding_mode_t<narrow_t>{};
using sut_t                  = wide_divider_t<narrow_t, fake_hardware_divider_t<narrow_t>>;

constexpr auto sut = sut_t{};

namespace fast_path {

constexpr auto dividend = wide_t{17};
constexpr auto divisor  = narrow_t{23};

// constexpr auto biased_dividend          = 57;              // 17*2 + 23
// constexpr auto hardware_division_result = result_t{2, 11}; // {57/23, 57%23}

constexpr auto expected = 292; // 2*5 + 23*7 + 11*11

static_assert(sut(dividend, divisor, rounding_mode) == expected);

} // namespace fast_path

namespace slow_path {

constexpr auto dividend = wide_t{18240};
constexpr auto divisor  = narrow_t{23};

// constexpr auto biased_dividend    = 36503;           // 18240*2 + 23
// constexpr auto high_dividend_high = 142;             // 36503/256
// constexpr auto high_dividend_low  = 151;             // 36503&255
// constexpr auto high_result        = result_t{6, 4};  // {142/23, 142%23}
// constexpr auto low_dividend       = 1175;            // 4*256 | 151
// constexpr auto low_result         = result_t{51, 2}; // {1175/23, 1175%23}
// constexpr auto quotient           = 1587;            // 6*256 + 51

constexpr auto expected = 8118; // 1587*5 + 23*7 + 2*11

static_assert(sut(dividend, divisor, rounding_mode) == expected);

} // namespace slow_path

} // namespace rounding_contract

// ====================================================================================================================
// Dispatch Boundaries
//
// Tests fast/slow dispatch decision is correct at threshold values.
// ====================================================================================================================

namespace dispatch_boundaries {

struct param_t
{
    using wide_t   = uint16_t;
    using narrow_t = uint8_t;

    static constexpr auto narrow_width = 8;
    static constexpr auto narrow_mask  = wide_t{0xFF};

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

struct division_divider_dispatch_test_t : TestWithParam<param_t>
{
    using wide_t   = param_t::wide_t;
    using narrow_t = param_t::narrow_t;

    using result_t = qr_pair_t<narrow_t>;

    static constexpr auto narrow_width = param_t::narrow_width;
    static constexpr auto narrow_mask  = param_t::narrow_mask;

    wide_t const   dividend               = GetParam().dividend;
    narrow_t const divisor                = GetParam().divisor;
    bool const     dispatches_to_hardware = GetParam().dispatches_to_hardware;

    static constexpr auto rounding_mode = stub_rounding_mode_t<narrow_t>{};

    struct mock_hardware_divider_t
    {
        MOCK_METHOD(result_t, divide, (wide_t dividend, narrow_t divisor), (const, noexcept));

        virtual ~mock_hardware_divider_t() = default;
    };
    StrictMock<mock_hardware_divider_t> mock_hardware_divider;

    struct hardware_divider_t
    {
        mock_hardware_divider_t* mock = nullptr;

        auto operator()(wide_t dividend, narrow_t divisor) const noexcept -> result_t
        {
            return mock->divide(dividend, divisor);
        }
    };

    result_t const expected_direct_result{17, 31};
    result_t const expected_high_result{19, 37};
    result_t const expected_low_result{23, 39};

    using sut_t = wide_divider_t<narrow_t, hardware_divider_t>;
    sut_t sut{hardware_divider_t{&mock_hardware_divider}};
};

TEST_P(division_divider_dispatch_test_t, result)
{
    wide_t expected;
    if (dispatches_to_hardware)
    {
        EXPECT_CALL(mock_hardware_divider, divide(dividend, divisor)).WillOnce(Return(expected_direct_result));
        expected = expected_direct_result.quotient;
    }
    else
    {
        EXPECT_CALL(mock_hardware_divider, divide(dividend >> narrow_width, divisor))
            .WillOnce(Return(expected_high_result));
        EXPECT_CALL(mock_hardware_divider, divide(int_cast<wide_t>((dividend & narrow_mask)
                                                                   | (expected_high_result.remainder << narrow_width)),
                                                  divisor))
            .WillOnce(Return(expected_low_result));

        expected = int_cast<wide_t>((expected_high_result.quotient << narrow_width) + expected_low_result.quotient);
    }

    auto const actual = sut(dividend, divisor, rounding_mode);

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
INSTANTIATE_TEST_SUITE_P(cases, division_divider_dispatch_test_t, ValuesIn(test_vectors));

} // namespace dispatch_boundaries

// ====================================================================================================================
// Correctness
//
// Tests division results against c++ integer division.
// ====================================================================================================================

template <typename t_narrow_t> struct divider_correctness_test_t : Test
{
    using narrow_t = t_narrow_t;
    using wide_t   = wider_t<narrow_t>;

    static constexpr auto narrow_width = int_cast<narrow_t>(sizeof(narrow_t) * CHAR_BIT);
    static constexpr auto max_narrow   = static_cast<narrow_t>(~narrow_t{0});

    struct checking_hardware_divider_t
    {
        auto operator()(wide_t dividend, narrow_t divisor) const noexcept -> qr_pair_t<narrow_t>
        {
            EXPECT_LT(dividend >> narrow_width, static_cast<wide_t>(divisor))
                << "hardware divider precondition violated";

            return {.quotient  = int_cast<narrow_t>(dividend / divisor),
                    .remainder = int_cast<narrow_t>(dividend % divisor)};
        }
    };

    static constexpr auto rounding_mode = stub_rounding_mode_t<narrow_t>{};

    using sut_t = wide_divider_t<narrow_t, checking_hardware_divider_t>;
    sut_t sut{};
};

using correctness_types_t = ::testing::Types<uint8_t, uint16_t, uint32_t>;
TYPED_TEST_SUITE(divider_correctness_test_t, correctness_types_t);

TYPED_TEST(divider_correctness_test_t, random_inputs)
{
    using wide_t   = TestFixture::wide_t;
    using narrow_t = TestFixture::narrow_t;

    constexpr auto narrow_width  = TestFixture::narrow_width;
    constexpr auto rounding_mode = TestFixture::rounding_mode;

    auto rng = std::mt19937{42};

    for (int i = 0; i < 10'000; ++i)
    {
        auto const hi       = static_cast<wide_t>(rng());
        auto const lo       = static_cast<narrow_t>(rng());
        auto const dividend = static_cast<wide_t>(static_cast<wide_t>(hi << narrow_width) | lo);
        auto const divisor  = static_cast<narrow_t>(rng() | 1);

        auto const expected = dividend / divisor;
        auto const actual   = this->sut(dividend, divisor, rounding_mode);

        ASSERT_EQ(expected, actual) << "dividend = " << dividend << " divisor = " << +divisor;
    }
}

TYPED_TEST(divider_correctness_test_t, edge_cases)
{
    using wide_t   = TestFixture::wide_t;
    using narrow_t = TestFixture::narrow_t;

    constexpr auto max_narrow    = TestFixture::max_narrow;
    constexpr auto max_wide      = static_cast<wide_t>(~wide_t{0});
    constexpr auto rounding_mode = TestFixture::rounding_mode;

    // zero dividend
    EXPECT_EQ(wide_t{0}, this->sut(wide_t{0}, narrow_t{1}, rounding_mode));
    EXPECT_EQ(wide_t{0}, this->sut(wide_t{0}, max_narrow, rounding_mode));

    // divisor = 1
    EXPECT_EQ(wide_t{max_narrow}, this->sut(wide_t{max_narrow}, narrow_t{1}, rounding_mode));

    // dividend = divisor (wide representation)
    EXPECT_EQ(wide_t{1}, this->sut(wide_t{max_narrow}, max_narrow, rounding_mode));

    // maximum dividend, maximum divisor
    EXPECT_EQ(max_wide / max_narrow, this->sut(max_wide, max_narrow, rounding_mode));

    // maximum dividend, divisor = 1
    EXPECT_EQ(max_wide, this->sut(max_wide, narrow_t{1}, rounding_mode));
}

} // namespace
} // namespace crv::division
