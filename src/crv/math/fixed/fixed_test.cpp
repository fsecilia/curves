// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/io.hpp>
#include <crv/math/io.hpp>
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <concepts>
#include <gmock/gmock.h>

namespace crv {
namespace {

using i_4_t = fixed_t<int_t, 4>;
using u_4_t = fixed_t<uint_t, 4>;

using i8_1_t     = fixed_t<int8_t, 1>;
using u8_1_t     = fixed_t<uint8_t, 1>;
using i8_2_t     = fixed_t<int8_t, 2>;
using i8_4_t     = fixed_t<int8_t, 4>;
using u8_4_t     = fixed_t<uint8_t, 4>;
using i8_6_t     = fixed_t<int8_t, 6>;
using i8_7_t     = fixed_t<int8_t, 7>;
using i16_0_t    = fixed_t<int16_t, 0>;
using i16_2_t    = fixed_t<int16_t, 2>;
using i16_4_t    = fixed_t<int16_t, 4>;
using u16_4_t    = fixed_t<uint16_t, 4>;
using i16_8_t    = fixed_t<int16_t, 8>;
using i16_9_t    = fixed_t<int16_t, 9>;
using u16_8_t    = fixed_t<uint16_t, 8>;
using i16_12_t   = fixed_t<int16_t, 12>;
using i32_0_t    = fixed_t<int32_t, 0>;
using i32_8_t    = fixed_t<int32_t, 8>;
using u32_8_t    = fixed_t<uint32_t, 8>;
using i32_16_t   = fixed_t<int32_t, 16>;
using i64_0_t    = fixed_t<int64_t, 0>;
using u64_0_t    = fixed_t<uint64_t, 0>;
using u64_4_t    = fixed_t<uint64_t, 4>;
using i64_63_t   = fixed_t<int64_t, 63>;
using u64_64_t   = fixed_t<uint64_t, 64>;
using i128_0_t   = fixed_t<int128_t, 0>;
using u128_0_t   = fixed_t<uint128_t, 0>;
using i128_8_t   = fixed_t<int128_t, 8>;
using u128_8_t   = fixed_t<uint128_t, 8>;
using i128_64_t  = fixed_t<int128_t, 64>;
using i128_126_t = fixed_t<int128_t, 126>;
using u128_128_t = fixed_t<uint128_t, 128>;

constexpr auto rne      = rounding_modes::shr::nearest_even;
constexpr auto truncate = rounding_modes::shr::truncate;

using value_t            = int_t;
constexpr auto frac_bits = 21;
using sut_t              = fixed_t<value_t, frac_bits>;

// Standard division is not constexpr. This type forces a specific type to go through the constexpr path.
using constexpr_div_sut_t = i16_4_t;

// ====================================================================================================================
// Test Fixtures
// ====================================================================================================================

struct fixed_test_t : Test
{};

struct fixed_test_with_rounding_mode_t : fixed_test_t
{
    struct mock_rounding_mode_t
    {
        MOCK_METHOD(int_t, bias, (int_t, int_t), (const, noexcept));
        MOCK_METHOD(int_t, carry, (int_t, int_t, int_t), (const, noexcept));
        virtual ~mock_rounding_mode_t() = default;
    };
    StrictMock<mock_rounding_mode_t> mock_rounding_mode{};

    struct rounding_mode_t
    {
        mock_rounding_mode_t* mock = nullptr;

        template <integral value_t> constexpr auto bias(value_t unshifted, int_t shift) const noexcept -> value_t
        {
            return mock->bias(int_cast<int_t>(unshifted), shift);
        }

        template <integral value_t>
        constexpr auto carry(value_t shifted, value_t unshifted, int_t shift) const noexcept -> value_t
        {
            return mock->carry(int_cast<int_t>(shifted), int_cast<int_t>(unshifted), shift);
        }
    };
    rounding_mode_t rounding_mode{&mock_rounding_mode};
};

// ====================================================================================================================
// Concepts
// ====================================================================================================================

static_assert(is_fixed<sut_t>);
static_assert(is_fixed<i8_4_t>);
static_assert(is_fixed<u16_4_t>);
static_assert(is_fixed<i16_4_t>);
static_assert(is_fixed<u128_0_t>);
static_assert(is_fixed<i128_64_t>);
static_assert(!is_fixed<int>);
static_assert(!is_fixed<int_t>);
static_assert(!is_fixed<float_t>);

struct not_fixed_t
{};

static_assert(!is_fixed<not_fixed_t>);

// ====================================================================================================================
// Type Traits
// ====================================================================================================================

static_assert(std::same_as<fixed::product_t<i16_0_t, fixed_t<int32_t, 4>>, fixed_t<int64_t, 4>>);
static_assert(std::same_as<fixed::product_t<fixed_t<uint16_t, 1>, fixed_t<int32_t, 4>>, fixed_t<int64_t, 5>>);
static_assert(std::same_as<fixed::product_t<fixed_t<int16_t, 3>, fixed_t<uint32_t, 4>>, fixed_t<int64_t, 7>>);
static_assert(std::same_as<fixed::product_t<fixed_t<uint16_t, 4>, fixed_t<uint32_t, 4>>, fixed_t<uint64_t, 8>>);

static_assert(std::same_as<fixed::product_t<fixed_t<int32_t, 16>, i16_0_t>, fixed_t<int64_t, 16>>);
static_assert(std::same_as<fixed::product_t<fixed_t<int32_t, 16>, fixed_t<uint16_t, 1>>, fixed_t<int64_t, 17>>);
static_assert(std::same_as<fixed::product_t<fixed_t<uint32_t, 16>, fixed_t<int16_t, 15>>, fixed_t<int64_t, 31>>);
static_assert(std::same_as<fixed::product_t<fixed_t<uint32_t, 16>, fixed_t<uint16_t, 16>>, fixed_t<uint64_t, 32>>);

// ====================================================================================================================
// Construction
// ====================================================================================================================

namespace construction {

static_assert(sut_t{}.value == 0);
static_assert(sut_t{0}.value == 0);
static_assert(sut_t{1}.value == 1 << frac_bits);
static_assert(sut_t{0xF1234}.value == 0xF1234ul << frac_bits);

} // namespace construction

// ====================================================================================================================
// Conversions
// ====================================================================================================================

namespace conversions {

// same precision, different size
static_assert(i16_4_t{i8_4_t::literal(10)}.value == 10);
static_assert(i8_4_t{i16_4_t::literal(10)}.value == 10);

// same size, different precision
static_assert(i8_4_t{i8_2_t::literal(10)}.value == 40);
static_assert(i8_2_t{i8_4_t::literal(40)}.value == 10);

// widen/narrow and precision changes
static_assert(i16_4_t{i8_2_t::literal(10)}.value == 40);
static_assert(i16_2_t{i8_4_t::literal(40)}.value == 10);
static_assert(i8_4_t{i16_2_t::literal(10)}.value == 40);
static_assert(i8_2_t{i16_4_t::literal(40)}.value == 10);

// widended range edge cases
static_assert(i16_9_t{i8_7_t::literal(64)}.value == 256);
static_assert(i8_7_t{i16_9_t::literal(256)}.value == 64);

namespace rounding {

using src_t        = i16_4_t;
using dst_t        = i8_2_t;
constexpr auto min = crv::min<int8_t>();
constexpr auto max = crv::max<int8_t>();

// default truncation
static_assert(dst_t{src_t::literal(min)}.value == min / 4);
static_assert(dst_t{src_t::literal(min + 1)}.value == min / 4);
static_assert(dst_t{src_t::literal(min + 4)}.value == min / 4 + 1);
static_assert(dst_t{src_t::literal(max - 4)}.value == max / 4 - 1);
static_assert(dst_t{src_t::literal(max)}.value == max / 4);

// round nearest even
static_assert(dst_t{src_t::literal(min), rne}.value == min / 4);
static_assert(dst_t{src_t::literal(min + 1), rne}.value == min / 4);
static_assert(dst_t{src_t::literal(min + 3), rne}.value == min / 4 + 1);
static_assert(dst_t{src_t::literal(max - 4), rne}.value == max / 4);
static_assert(dst_t{src_t::literal(max), rne}.value == max / 4 + 1);

} // namespace rounding

namespace operator_bool {

// signed bool conversion
static_assert(i8_4_t::literal(min<int8_t>()));
static_assert(i8_4_t::literal(-1));
static_assert(!i8_4_t::literal(0));
static_assert(i8_4_t::literal(1));
static_assert(i8_4_t::literal(max<int8_t>()));

// unsigned bool conversion
static_assert(!u8_4_t::literal(0));
static_assert(u8_4_t::literal(1));
static_assert(u8_4_t::literal(max<uint8_t>()));

} // namespace operator_bool

} // namespace conversions

// ====================================================================================================================
// Comparisons
// ====================================================================================================================

namespace comparisons {

static_assert(sut_t{5} == sut_t{5});
static_assert(sut_t{3} != sut_t{7});
static_assert(sut_t{3} < sut_t{7});
static_assert(sut_t{-3} < sut_t{3});

} // namespace comparisons

// ====================================================================================================================
// Unary Arithmetic
// ====================================================================================================================

namespace unary_arithmetic {

static_assert(+sut_t{10} == sut_t{10});
static_assert(+sut_t{-10} == sut_t{-10});

static_assert(-sut_t{10} == sut_t{-10});
static_assert(-sut_t{-10} == sut_t{10});

} // namespace unary_arithmetic

// ====================================================================================================================
// Scalar Arithmetic
// ====================================================================================================================

namespace scalar_arithmetic {

static_assert(sut_t{10} + 3 == sut_t{13});
static_assert(3 + sut_t{10} == sut_t{13});

static_assert(sut_t{10} - 3 == sut_t{7});
static_assert(10 - sut_t{3} == sut_t{7});
static_assert(sut_t{10} - 3 != 3 - sut_t{10});

static_assert(sut_t{10} * 3 == sut_t{30});
static_assert(3 * sut_t{10} == sut_t{30});

static_assert(constexpr_div_sut_t{30} / 3 == constexpr_div_sut_t{10});
static_assert(30 / constexpr_div_sut_t{3} == constexpr_div_sut_t{10});
static_assert(constexpr_div_sut_t{12} / 3 != 3 / constexpr_div_sut_t{12});

static_assert((i16_8_t{16} >> 1) == i16_8_t{8});
static_assert((i16_8_t{16} >> 2) == i16_8_t{4});
static_assert((i16_8_t::literal(max<i16_8_t::value_t>()) >> 1)
              == i16_8_t::literal(int_cast<i16_8_t::value_t>(max<i16_8_t::value_t>() >> 1)));
static_assert((i16_8_t::literal(max<i16_8_t::value_t>()) >> 2)
              == i16_8_t::literal(int_cast<i16_8_t::value_t>(max<i16_8_t::value_t>() >> 2)));

static_assert((u16_8_t{16} >> 1) == u16_8_t{8});
static_assert((u16_8_t{16} >> 2) == u16_8_t{4});
static_assert((u16_8_t::literal(max<u16_8_t::value_t>()) >> 1)
              == u16_8_t::literal(int_cast<u16_8_t::value_t>(max<u16_8_t::value_t>() >> 1)));
static_assert((u16_8_t::literal(max<u16_8_t::value_t>()) >> 2)
              == u16_8_t::literal(int_cast<u16_8_t::value_t>(max<u16_8_t::value_t>() >> 2)));

static_assert((i16_8_t{16} << 1) == i16_8_t{32});
static_assert((i16_8_t{16} << 2) == i16_8_t{64});
static_assert((i16_8_t::literal(max<i16_8_t::value_t>()) << 1)
              == i16_8_t::literal(static_cast<i16_8_t::value_t>(max<i16_8_t::value_t>() << 1)));
static_assert((i16_8_t::literal(max<i16_8_t::value_t>()) << 2)
              == i16_8_t::literal(static_cast<i16_8_t::value_t>(max<i16_8_t::value_t>() << 2)));

static_assert((u16_8_t{16} << 1) == u16_8_t{32});
static_assert((u16_8_t{16} << 2) == u16_8_t{64});
static_assert((u16_8_t::literal(max<u16_8_t::value_t>()) << 1)
              == u16_8_t::literal(static_cast<u16_8_t::value_t>(max<u16_8_t::value_t>() << 1)));
static_assert((u16_8_t::literal(max<u16_8_t::value_t>()) << 2)
              == u16_8_t::literal(static_cast<u16_8_t::value_t>(max<u16_8_t::value_t>() << 2)));

} // namespace scalar_arithmetic

// --------------------------------------------------------------------------------------------------------------------
// Compound Assignment
// --------------------------------------------------------------------------------------------------------------------

namespace compound_assignment {

static_assert([] {
    auto val = sut_t{10};
    return &val == &(val += 3);
}());

static_assert([] {
    auto val = sut_t{10};
    val += 3;
    return val;
}() == sut_t{13});

static_assert([] {
    auto val = sut_t{10};
    return &val == &(val -= 3);
}());

static_assert([] {
    auto val = sut_t{10};
    val -= 3;
    return val;
}() == sut_t{7});

static_assert([] {
    auto val = sut_t{10};
    return &val == &(val *= 2);
}());

static_assert([] {
    auto val = sut_t{10};
    val *= 2;
    return val;
}() == sut_t{20});

static_assert([] {
    auto val = sut_t{10};
    return &val == &(val /= 2);
}());

static_assert([] {
    auto val = sut_t{10};
    val /= 2;
    return val;
}() == sut_t{5});

static_assert([] {
    auto val = sut_t{10};
    return &val == &(val %= 3);
}());

static_assert([] {
    auto val = sut_t{10};
    val %= 3;
    return val;
}() == sut_t{1});

static_assert([] {
    auto val = sut_t{80};
    return &val == &(val >>= 3);
}());

static_assert([] {
    auto val = sut_t{80};
    val >>= 3;
    return val;
}() == sut_t{10});

static_assert([] {
    auto val = sut_t{80};
    return &val == &(val <<= 3);
}());

static_assert([] {
    auto val = sut_t{80};
    val <<= 3;
    return val;
}() == sut_t{640});

} // namespace compound_assignment

// ====================================================================================================================
// Fixed Arithmetic
// ====================================================================================================================

namespace fixed_arithmetic {

// --------------------------------------------------------------------------------------------------------------------
// Addition
// --------------------------------------------------------------------------------------------------------------------

static_assert(sut_t{3} + sut_t{7} == sut_t{10});
static_assert(sut_t{-3} + sut_t{7} == sut_t{4});
static_assert(sut_t{3} + sut_t{-7} == sut_t{-4});
static_assert(sut_t{-3} + sut_t{-7} == sut_t{-10});

// unsigned wrap-around
static_assert(u16_4_t::literal(crv::max<uint16_t>()) + u16_4_t ::literal(1) == u16_4_t ::literal(0));

// --------------------------------------------------------------------------------------------------------------------
// Subtraction
// --------------------------------------------------------------------------------------------------------------------

static_assert(sut_t{3} - sut_t{7} == sut_t{-4});
static_assert(sut_t{-3} - sut_t{7} == sut_t{-10});
static_assert(sut_t{3} - sut_t{-7} == sut_t{10});
static_assert(sut_t{-3} - sut_t{-7} == sut_t{4});

// unsigned wrap-around
static_assert(u16_4_t ::literal(0) - u16_4_t ::literal(1) == u16_4_t ::literal(crv::max<uint16_t>()));

// --------------------------------------------------------------------------------------------------------------------
// Exact Multiplication
// --------------------------------------------------------------------------------------------------------------------

// mixed types, zeros
static_assert(multiply(i8_4_t{-5}, i16_4_t{0}) == i32_8_t{0});
static_assert(multiply(i8_4_t{0}, i16_4_t{-7}) == i32_8_t{0});
static_assert(multiply(i8_4_t{0}, i16_4_t{0}) == i32_8_t{0});
static_assert(multiply(i8_4_t{0}, i16_4_t{7}) == i32_8_t{0});
static_assert(multiply(i8_4_t{5}, i16_4_t{0}) == i32_8_t{0});

// mixed types, signed and unsigned
static_assert(multiply(i8_4_t{5}, i16_4_t{7}) == i32_8_t{5 * 7});
static_assert(multiply(i8_4_t{5}, u16_4_t{7}) == i32_8_t{5 * 7});
static_assert(multiply(u8_4_t{5}, i16_4_t{7}) == i32_8_t{5 * 7});
static_assert(multiply(u8_4_t{5}, u16_4_t{7}) == u32_8_t{5 * 7});

// mixed types with 128-bit results
static_assert(multiply(i8_4_t{5}, u64_4_t{7}) == i128_8_t{5 * 7});
static_assert(multiply(u8_4_t{5}, u64_4_t{7}) == u128_8_t{5 * 7});

// mixed signs
static_assert(multiply(i8_4_t{-5}, u16_4_t{7}) == i32_8_t{-5 * 7});
static_assert(multiply(i8_4_t{-5}, i16_4_t{-7}) == i32_8_t{5 * 7});

// pure integer parts
static_assert(multiply(i16_0_t{3}, i32_0_t{5}) == i64_0_t{15});

// range limits
static_assert(multiply(i8_6_t::literal(min<int8_t>()), i8_6_t::literal(min<int8_t>()))
              == i16_12_t::literal(min<int8_t>() * min<int8_t>()));
static_assert(multiply(i8_6_t::literal(min<int8_t>()), i8_6_t::literal(max<int8_t>()))
              == i16_12_t::literal(min<int8_t>() * max<int8_t>()));
static_assert(multiply(i8_6_t::literal(max<int8_t>()), i8_6_t::literal(min<int8_t>()))
              == i16_12_t::literal(max<int8_t>() * min<int8_t>()));
static_assert(multiply(i8_6_t::literal(max<int8_t>()), i8_6_t::literal(max<int8_t>()))
              == i16_12_t::literal(max<int8_t>() * max<int8_t>()));

// 128-bit limits
static_assert(multiply(i64_0_t::literal(max<int64_t>()), i64_0_t::literal(max<int64_t>()))
              == i128_0_t::literal(int128_t{max<int64_t>()} * max<int64_t>()));
static_assert(multiply(u64_0_t(max<uint64_t>()), u64_0_t::literal(max<uint64_t>()))
              == u128_0_t::literal(uint128_t{max<uint64_t>()} * max<uint64_t>()));
static_assert(multiply(i64_63_t::literal(max<int64_t>()), i64_63_t::literal(max<int64_t>()))
              == i128_126_t::literal(int128_t{max<int64_t>()} * max<int64_t>()));
static_assert(multiply(u64_64_t::literal(max<uint64_t>()), u64_64_t::literal(max<uint64_t>()))
              == u128_128_t::literal(uint128_t{max<uint64_t>()} * max<uint64_t>()));

// --------------------------------------------------------------------------------------------------------------------
// Multiplication to Specific Type with Rounding Mode
// --------------------------------------------------------------------------------------------------------------------

static_assert(multiply<i8_1_t>(fixed_t<int16_t, 1>{2}, fixed_t<int32_t, 1>{3}, truncate) == i8_1_t{2 * 3});
static_assert(multiply<i8_1_t>(fixed_t<int16_t, 1>{2}, fixed_t<uint32_t, 1>{3}, truncate) == i8_1_t{2 * 3});
static_assert(multiply<i8_1_t>(fixed_t<uint16_t, 1>{2}, fixed_t<int32_t, 1>{3}, truncate) == i8_1_t{2 * 3});
static_assert(multiply<i8_1_t>(fixed_t<uint16_t, 1>{2}, fixed_t<uint32_t, 1>{3}, truncate) == i8_1_t{2 * 3});

TEST_F(fixed_test_with_rounding_mode_t, multiplication_to_specific_type)
{
    using out_t              = fixed_t<uint_t, 1>;
    auto const lhs           = u8_1_t{2};
    auto const rhs           = fixed_t<int16_t, 1>{3};
    auto const expected_bias = uint32_t{23};
    auto const expected      = out_t::literal(29);
    EXPECT_CALL(mock_rounding_mode, bias((2 * 3) << 2, 1)).WillOnce(Return(expected_bias));
    EXPECT_CALL(mock_rounding_mode, carry(expected_bias >> 1, expected_bias, 1)).WillOnce(Return(expected.value));

    auto const actual = multiply<out_t>(lhs, rhs, rounding_mode);

    EXPECT_EQ(expected, actual);
}

// --------------------------------------------------------------------------------------------------------------------
// Multiplication to LHS Type with Rounding Mode
// --------------------------------------------------------------------------------------------------------------------

static_assert(multiply<i8_1_t>(i8_1_t{2}, i8_1_t{3}, truncate) == i8_1_t{2 * 3});
static_assert(multiply<i8_1_t>(i8_1_t{2}, u8_1_t{3}, truncate) == i8_1_t{2 * 3});
static_assert(multiply<u8_1_t>(u8_1_t{2}, i8_1_t{3}, truncate) == u8_1_t{2 * 3});
static_assert(multiply<u8_1_t>(u8_1_t{2}, u8_1_t{3}, truncate) == u8_1_t{2 * 3});

TEST_F(fixed_test_with_rounding_mode_t, multiplication_to_lhs_type)
{
    using sut_t              = fixed_t<int_t, 1>;
    auto const lhs           = sut_t::literal(2 << 1);
    auto const rhs           = sut_t::literal(3 << 1);
    auto const expected_bias = sut_t::value_t{23};
    auto const expected      = sut_t::literal(29);
    EXPECT_CALL(mock_rounding_mode, bias((2 * 3) << 2, 1)).WillOnce(Return(expected_bias));
    EXPECT_CALL(mock_rounding_mode, carry(expected_bias >> 1, expected_bias, 1)).WillOnce(Return(expected.value));

    auto const actual = multiply<sut_t>(lhs, rhs, rounding_mode);

    EXPECT_EQ(expected, actual);
}

// --------------------------------------------------------------------------------------------------------------------
// Division
// --------------------------------------------------------------------------------------------------------------------

namespace division {

// same-type signed
static_assert(divide<i16_8_t>(i16_8_t{6}, i16_8_t{2}) == i16_8_t{3});
static_assert(divide<i16_8_t>(i16_8_t{-6}, i16_8_t{2}) == i16_8_t{-3});
static_assert(divide<i16_8_t>(i16_8_t{6}, i16_8_t{-2}) == i16_8_t{-3});
static_assert(divide<i16_8_t>(i16_8_t{-6}, i16_8_t{-2}) == i16_8_t{3});

// identity and zero dividend
static_assert(divide<i16_8_t>(i16_8_t{5}, i16_8_t{1}) == i16_8_t{5});
static_assert(divide<i16_8_t>(i16_8_t{0}, i16_8_t{5}) == i16_8_t{0});

// truncation toward zero: floor(2/3 * 256) = 170
static_assert(divide<i16_8_t>(i16_8_t{2}, i16_8_t{3}).value == 170);
static_assert(divide<i16_8_t>(i16_8_t{-2}, i16_8_t{3}).value == -170);

// same-type unsigned
static_assert(divide<u16_8_t>(u16_8_t{6}, u16_8_t{2}) == u16_8_t{3});
static_assert(divide<u16_8_t>(u16_8_t{0}, u16_8_t{5}) == u16_8_t{0});
static_assert(divide<u16_8_t>(u16_8_t{2}, u16_8_t{3}).value == 170);

// mixed sign
static_assert(divide<i16_8_t>(u16_8_t{6}, i16_8_t{-2}) == i16_8_t{-3});
static_assert(divide<i16_8_t>(i16_8_t{-6}, u16_8_t{2}) == i16_8_t{-3});
static_assert(divide<i16_8_t>(u16_8_t{6}, i16_8_t{2}) == i16_8_t{3});

// unsigned output from mixed-sign: negative mathematical result clamps to 0
static_assert(divide<u16_8_t>(i16_8_t{-6}, u16_8_t{2}) == u16_8_t{0});

// mixed precision
static_assert(divide<i16_8_t>(i16_4_t{6}, i16_12_t{2}) == i16_8_t{3});
static_assert(divide<i16_8_t>(i16_4_t{-6}, i16_12_t{2}) == i16_8_t{-3});

// output type widening
static_assert(divide<i16_4_t>(i16_8_t{6}, i16_8_t{2}) == i16_4_t{3});

// output type narrowing
static_assert(divide<i16_8_t>(i16_4_t{6}, i16_4_t{2}) == i16_8_t{3});

// rounding mode
static_assert(divide<i16_8_t>(i16_8_t{2}, i16_8_t{3}).value == 170);
static_assert(divide<i16_8_t>(i16_8_t{2}, i16_8_t{3}, rounding_modes::div::nearest_away).value == 171);

// rounding composes with sign
static_assert(divide<i16_8_t>(i16_8_t{-2}, i16_8_t{3}, rounding_modes::div::nearest_away).value == -171);

// saturation

// divide by zero
static_assert(divide<i16_8_t>(i16_8_t{5}, i16_8_t{0}) == i16_8_t::literal(max<int16_t>()));
static_assert(divide<i16_8_t>(i16_8_t{-5}, i16_8_t{0}) == i16_8_t::literal(min<int16_t>()));
static_assert(divide<i16_8_t>(i16_8_t{0}, i16_8_t{0}) == i16_8_t{0});

// unsigned divide by zero
static_assert(divide<u16_8_t>(u16_8_t{5}, u16_8_t{0}) == u16_8_t::literal(max<uint16_t>()));
static_assert(divide<u16_8_t>(u16_8_t{0}, u16_8_t{0}) == u16_8_t{0});

// positive overflow saturates to max
static_assert(divide<i8_4_t>(i16_8_t{100}, i16_8_t{1}) == i8_4_t::literal(max<int8_t>()));

// negative overflow saturates to min
static_assert(divide<i8_4_t>(i16_8_t{-100}, i16_8_t{1}) == i8_4_t::literal(min<int8_t>()));

// literal operators
struct fixed_division_ops_t : Test
{
    using sut_t = i16_8_t;
    sut_t lhs   = sut_t{6};
    sut_t rhs   = sut_t{2};
};

TEST_F(fixed_division_ops_t, binary_op)
{
    EXPECT_EQ(sut_t{3}, lhs / rhs);
}

TEST_F(fixed_division_ops_t, truncation)
{
    auto const actual = sut_t{7} / sut_t{3};
    EXPECT_EQ(597, actual.value);
}

// operator/ with mixed fixed
TEST_F(fixed_division_ops_t, mixed_type_binary_op)
{
    auto const rhs_q4 = i16_4_t{2};
    auto const actual = lhs / rhs_q4;

    static_assert(std::same_as<decltype(actual), i16_8_t const>);
    EXPECT_EQ(sut_t{3}, actual);
}

TEST_F(fixed_division_ops_t, mixed_type_binary_op_unsigned_rhs)
{
    auto const rhs_unsigned = u16_8_t{2};
    auto const actual       = lhs / rhs_unsigned;

    static_assert(std::same_as<decltype(actual), i16_8_t const>);
    EXPECT_EQ(sut_t{3}, actual);
}

template <int t_out_frac_bits, int t_lhs_frac_bits, int t_rhs_frac_bits> struct vector_t
{
    static constexpr auto out_frac_bits = t_out_frac_bits;
    static constexpr auto lhs_frac_bits = t_lhs_frac_bits;
    static constexpr auto rhs_frac_bits = t_rhs_frac_bits;

    std::string name;
    uint64_t    lhs;
    uint64_t    rhs;
    uint64_t    expected;

    friend auto operator<<(std::ostream& out, vector_t const& src) -> std::ostream&
    {
        return out << "{.name = \"" << src.name << "\", .lhs = " << src.lhs << "@" << lhs_frac_bits
                   << ", .rhs = " << src.rhs << "@" << rhs_frac_bits << ", .expected = " << src.expected << "@"
                   << out_frac_bits << "}";
    }
};

struct fixed_division_test_t : Test
{
    template <typename vector_t> void test(vector_t const& vector)
    {
        using out_t         = fixed_t<uint64_t, vector_t::out_frac_bits>;
        auto const lhs      = fixed_t<uint64_t, vector_t::lhs_frac_bits>::literal(vector.lhs);
        auto const rhs      = fixed_t<uint64_t, vector_t::rhs_frac_bits>::literal(vector.rhs);
        auto const expected = out_t::literal(vector.expected);

        auto const actual = divide<out_t>(lhs, rhs);

        EXPECT_EQ(actual.value, expected.value) << "failed for " << vector;
    }
};

TEST_F(fixed_division_test_t, limits)
{
    test(vector_t<0, 0, 0>{"safe max", max<uint64_t>(), 1, max<uint64_t>()});
    test(vector_t<60, 0, 0>{"valid high bit", 16, 2, 1ULL << 63});
    test(vector_t<60, 0, 0>{"saturates 16 << 60", 16, 1, max<uint64_t>()});
    test(vector_t<64, 0, 0>{"saturates 1 << 64", 1, 1, max<uint64_t>()});
}

static constexpr auto lhs_frac_bits = 3;
static constexpr auto rhs_frac_bits = 5;
static constexpr auto out_frac_bits = 20;
using specialized_vector_t          = vector_t<out_frac_bits, lhs_frac_bits, rhs_frac_bits>;

struct fixed_division_vector_test_t : fixed_division_test_t, WithParamInterface<specialized_vector_t>
{};

TEST_P(fixed_division_vector_test_t, result)
{
    test(GetParam());
}

specialized_vector_t const vectors[] = {
    {"0/1", 0 << lhs_frac_bits, 1 << rhs_frac_bits, (0 << out_frac_bits) / 1},
    {"1/1", 1 << lhs_frac_bits, 1 << rhs_frac_bits, (1 << out_frac_bits) / 1},
    {"2/1", 2 << lhs_frac_bits, 1 << rhs_frac_bits, (2 << out_frac_bits) / 1},
    {"0/2", 0 << lhs_frac_bits, 2 << rhs_frac_bits, (0 << out_frac_bits) / 2},
    {"1/2", 1 << lhs_frac_bits, 2 << rhs_frac_bits, (1 << out_frac_bits) / 2},
    {"2/2", 2 << lhs_frac_bits, 2 << rhs_frac_bits, (2 << out_frac_bits) / 2},
    {"3/2", 3 << lhs_frac_bits, 2 << rhs_frac_bits, (3 << out_frac_bits) / 2},
    {"0/3", 0 << lhs_frac_bits, 3 << rhs_frac_bits, (0 << out_frac_bits) / 3},
    {"1/3", 1 << lhs_frac_bits, 3 << rhs_frac_bits, (1 << out_frac_bits) / 3},
    {"2/3", 2 << lhs_frac_bits, 3 << rhs_frac_bits, (2 << out_frac_bits) / 3},
    {"3/3", 3 << lhs_frac_bits, 3 << rhs_frac_bits, (3 << out_frac_bits) / 3},
    {"4/3", 4 << lhs_frac_bits, 3 << rhs_frac_bits, (4 << out_frac_bits) / 3},
    {"0/4", 0 << lhs_frac_bits, 4 << rhs_frac_bits, (0 << out_frac_bits) / 4},
    {"1/4", 1 << lhs_frac_bits, 4 << rhs_frac_bits, (1 << out_frac_bits) / 4},
    {"2/4", 2 << lhs_frac_bits, 4 << rhs_frac_bits, (2 << out_frac_bits) / 4},
    {"3/4", 3 << lhs_frac_bits, 4 << rhs_frac_bits, (3 << out_frac_bits) / 4},
    {"4/4", 4 << lhs_frac_bits, 4 << rhs_frac_bits, (4 << out_frac_bits) / 4},
    {"5/4", 5 << lhs_frac_bits, 4 << rhs_frac_bits, (5 << out_frac_bits) / 4},
    {"0/5", 0 << lhs_frac_bits, 5 << rhs_frac_bits, (0 << out_frac_bits) / 5},
    {"1/5", 1 << lhs_frac_bits, 5 << rhs_frac_bits, (1 << out_frac_bits) / 5},
    {"2/5", 2 << lhs_frac_bits, 5 << rhs_frac_bits, (2 << out_frac_bits) / 5},
    {"3/5", 3 << lhs_frac_bits, 5 << rhs_frac_bits, (3 << out_frac_bits) / 5},
    {"4/5", 4 << lhs_frac_bits, 5 << rhs_frac_bits, (4 << out_frac_bits) / 5},
    {"5/5", 5 << lhs_frac_bits, 5 << rhs_frac_bits, (5 << out_frac_bits) / 5},
    {"6/5", 6 << lhs_frac_bits, 5 << rhs_frac_bits, (6 << out_frac_bits) / 5},
};
INSTANTIATE_TEST_SUITE_P(cases, fixed_division_vector_test_t, ValuesIn(vectors),
                         test_name_generator_t<specialized_vector_t>{});

} // namespace division

// ====================================================================================================================
// Modulo
// ====================================================================================================================

namespace modulo {

// dividend sign
static_assert(i16_8_t{5} % i16_8_t{3} == i16_8_t{2});
static_assert(i16_8_t{-5} % i16_8_t{3} == i16_8_t{-2});
static_assert(i16_8_t{5} % i16_8_t{-3} == i16_8_t{2});
static_assert(i16_8_t{-5} % i16_8_t{-3} == i16_8_t{-2});

// fractional divisor
static_assert(i16_8_t::literal(704) % i16_8_t::literal(128) == i16_8_t::literal(64));

// dividend < divisor
static_assert(i16_8_t{2} % i16_8_t{5} == i16_8_t{2});
static_assert(i16_8_t::literal(128) % i16_8_t{5} == i16_8_t::literal(128)); // 0.5 % 5 == 0.5

// exact divisibility
static_assert(i16_8_t{6} % i16_8_t{2} == i16_8_t{0});
static_assert(i16_8_t::literal(768) % i16_8_t::literal(256) == i16_8_t{0}); // 3.0 % 1.0

// extreme radix disparity
static_assert(i32_0_t{5} % i32_16_t{2} == i32_0_t{1});
static_assert(i32_16_t{5} % i32_0_t{2} == i32_16_t{1});

// narrowing truncation
static_assert(mod<i8_1_t>(i8_4_t::literal(34), i8_4_t{1}) == i8_1_t{0});

// rounding via mock
TEST_F(fixed_test_with_rounding_mode_t, modulo_to_specific_type)
{
    using out_t              = fixed_t<uint_t, 1>;
    auto const lhs           = fixed_t<uint8_t, 3>::literal(27); // 3.375
    auto const rhs           = fixed_t<int16_t, 3>::literal(12); // 1.5
    auto const remainder     = int_t{27 % 12};                   // 3
    auto const expected_bias = uint32_t{11};
    auto const expected      = out_t::literal(5);

    // out_shift = max(3, 3) - 1 = 2
    EXPECT_CALL(mock_rounding_mode, bias(remainder, 2)).WillOnce(Return(expected_bias));
    EXPECT_CALL(mock_rounding_mode, carry(expected_bias >> 2, expected_bias, 2)).WillOnce(Return(expected.value));

    auto const actual = mod<out_t>(lhs, rhs, rounding_mode);

    EXPECT_EQ(expected, actual);
}

} // namespace modulo

// --------------------------------------------------------------------------------------------------------------------
// Compound Assignment
// --------------------------------------------------------------------------------------------------------------------

static_assert([] {
    auto val = sut_t{10};
    return &val == &(val += sut_t{3});
}());

static_assert([] {
    auto val = sut_t{10};
    val += sut_t{3};
    return val;
}() == sut_t{13});

static_assert([] {
    auto val = sut_t{10};
    return &val == &(val -= sut_t{3});
}());

static_assert([] {
    auto val = sut_t{10};
    val -= sut_t{3};
    return val;
}() == sut_t{7});

static_assert([] {
    auto val = sut_t{10};
    return &val == &(val *= sut_t{2});
}());

static_assert([] {
    auto val = sut_t{10};
    val *= sut_t{2};
    return val;
}() == sut_t{20});

static_assert([] {
    auto val = constexpr_div_sut_t{10};
    return &val == &(val /= constexpr_div_sut_t{2});
}());

static_assert([] {
    auto val = constexpr_div_sut_t{10};
    val /= constexpr_div_sut_t{2};
    return val;
}() == constexpr_div_sut_t{5});

static_assert([] {
    auto val = sut_t{10};
    return &val == &(val %= sut_t{3});
}());

static_assert([] {
    auto val = sut_t{10};
    val %= sut_t{3};
    return val;
}() == sut_t{1});

} // namespace fixed_arithmetic

// ====================================================================================================================
// Extraction and Rounding
// ====================================================================================================================

namespace extraction {

constexpr auto p_2_00 = i16_4_t::literal(32);
constexpr auto p_2_25 = i16_4_t::literal(36);
constexpr auto p_2_50 = i16_4_t::literal(40);
constexpr auto p_2_75 = i16_4_t::literal(44);
constexpr auto p_3_00 = i16_4_t::literal(48);

constexpr auto p_0_00 = i16_4_t::literal(0);
constexpr auto p_0_25 = i16_4_t::literal(4);
constexpr auto p_0_50 = i16_4_t::literal(8);
constexpr auto p_0_75 = i16_4_t::literal(12);

constexpr auto n_2_00 = i16_4_t::literal(-32);
constexpr auto n_2_25 = i16_4_t::literal(-36);
constexpr auto n_2_50 = i16_4_t::literal(-40);
constexpr auto n_2_75 = i16_4_t::literal(-44);
constexpr auto n_3_00 = i16_4_t::literal(-48);

constexpr auto p_int = i16_0_t::literal(5);
constexpr auto n_int = i16_0_t::literal(-5);

// ----------------------------------------------------------------------------------------------------------------
// Ceil
// ----------------------------------------------------------------------------------------------------------------

static_assert(ceil(p_2_00) == p_2_00);
static_assert(ceil(p_2_25) == p_3_00);
static_assert(ceil(p_2_75) == p_3_00);

static_assert(ceil(n_2_00) == n_2_00);
static_assert(ceil(n_2_25) == n_2_00);
static_assert(ceil(n_2_75) == n_2_00);

static_assert(ceil(p_int) == p_int);
static_assert(ceil(n_int) == n_int);

// ----------------------------------------------------------------------------------------------------------------
// Floor
// ----------------------------------------------------------------------------------------------------------------

static_assert(floor(p_2_00) == p_2_00);
static_assert(floor(p_2_25) == p_2_00);
static_assert(floor(p_2_75) == p_2_00);

static_assert(floor(n_2_00) == n_2_00);
static_assert(floor(n_2_25) == n_3_00);
static_assert(floor(n_2_75) == n_3_00);

static_assert(floor(p_int) == p_int);
static_assert(floor(n_int) == n_int);

// ----------------------------------------------------------------------------------------------------------------
// Frac
// ----------------------------------------------------------------------------------------------------------------

static_assert(frac(p_2_00) == p_0_00);
static_assert(frac(p_2_25) == p_0_25);
static_assert(frac(p_2_50) == p_0_50);
static_assert(frac(p_2_75) == p_0_75);

static_assert(frac(n_2_00) == p_0_00);
static_assert(frac(n_2_25) == p_0_75);
static_assert(frac(n_2_50) == p_0_50);
static_assert(frac(n_2_75) == p_0_25);

static_assert(frac(p_int) == i16_0_t{0});
static_assert(frac(n_int) == i16_0_t{0});

} // namespace extraction

// ====================================================================================================================
// Math Functions
// ====================================================================================================================

namespace math_functions {

static_assert(abs(i_4_t::literal(-max<int_t>())) == i_4_t::literal(max<int_t>()));
static_assert(abs(i_4_t::literal(-1)) == i_4_t::literal(1));
static_assert(abs(i_4_t::literal(0)) == i_4_t::literal(0));
static_assert(abs(i_4_t::literal(1)) == i_4_t::literal(1));
static_assert(abs(i_4_t::literal(max<int_t>())) == i_4_t::literal(max<int_t>()));

static_assert(abs(u_4_t::literal(0)) == u_4_t::literal(0));
static_assert(abs(u_4_t::literal(1)) == u_4_t::literal(1));
static_assert(abs(u_4_t{max<int_t>()}) == u_4_t{max<int_t>()});

} // namespace math_functions

namespace numeric_limits_tests {

using value_t = int32_t;
using sut_t   = std::numeric_limits<i32_16_t>;
using base_t  = std::numeric_limits<value_t>;

// traits
static_assert(sut_t::is_specialized);
static_assert(!sut_t::is_integer);
static_assert(sut_t::is_exact);
static_assert(sut_t::is_signed == base_t::is_signed);
static_assert(!sut_t::has_infinity);
static_assert(!sut_t::has_quiet_NaN);

// values
static_assert(sut_t::max().value == base_t::max());
static_assert(sut_t::lowest().value == base_t::lowest());
static_assert(sut_t::min().value == 1); // min() is the smallest positive value, not the most negative

// epsilon is the constant step size
static_assert(sut_t::epsilon().value == 1);

} // namespace numeric_limits_tests

} // namespace
} // namespace crv
