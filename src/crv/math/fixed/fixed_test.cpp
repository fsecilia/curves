// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/io.hpp>
#include <crv/math/io.hpp>
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <crv/test/typed_equal.hpp>
#include <concepts>
#include <gmock/gmock.h>

namespace crv {
namespace {

using value_t            = int_t;
constexpr auto frac_bits = 21;
using sut_t              = fixed_t<value_t, frac_bits>;

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
static_assert(is_fixed<fixed_q15_0_t>);
static_assert(is_fixed<fixed_q32_32_t>);
static_assert(is_fixed<fixed_q0_64_t>);
static_assert(!is_fixed<int>);
static_assert(!is_fixed<int_t>);
static_assert(!is_fixed<float_t>);

struct not_fixed_t
{};

static_assert(!is_fixed<not_fixed_t>);

// ====================================================================================================================
// Type Traits
// ====================================================================================================================

static_assert(std::same_as<fixed::promoted_t<fixed_t<int16_t, 0>, fixed_t<int32_t, 4>>, fixed_t<int32_t, 4>>);
static_assert(std::same_as<fixed::promoted_t<fixed_t<int16_t, 1>, fixed_t<uint32_t, 4>>, fixed_t<int32_t, 4>>);
static_assert(std::same_as<fixed::promoted_t<fixed_t<uint16_t, 3>, fixed_t<int32_t, 4>>, fixed_t<int32_t, 4>>);
static_assert(std::same_as<fixed::promoted_t<fixed_t<uint16_t, 4>, fixed_t<uint32_t, 4>>, fixed_t<uint32_t, 4>>);

static_assert(std::same_as<fixed::promoted_t<fixed_t<int32_t, 16>, fixed_t<int16_t, 0>>, fixed_t<int32_t, 16>>);
static_assert(std::same_as<fixed::promoted_t<fixed_t<int32_t, 16>, fixed_t<uint16_t, 1>>, fixed_t<int32_t, 16>>);
static_assert(std::same_as<fixed::promoted_t<fixed_t<uint32_t, 16>, fixed_t<int16_t, 15>>, fixed_t<int32_t, 16>>);
static_assert(std::same_as<fixed::promoted_t<fixed_t<uint32_t, 16>, fixed_t<uint16_t, 16>>, fixed_t<uint32_t, 16>>);

static_assert(std::same_as<fixed::wider_t<fixed_t<int16_t, 0>, fixed_t<int32_t, 4>>, fixed_t<int64_t, 4>>);
static_assert(std::same_as<fixed::wider_t<fixed_t<uint16_t, 1>, fixed_t<int32_t, 4>>, fixed_t<int64_t, 5>>);
static_assert(std::same_as<fixed::wider_t<fixed_t<int16_t, 3>, fixed_t<uint32_t, 4>>, fixed_t<int64_t, 7>>);
static_assert(std::same_as<fixed::wider_t<fixed_t<uint16_t, 4>, fixed_t<uint32_t, 4>>, fixed_t<uint64_t, 8>>);

static_assert(std::same_as<fixed::wider_t<fixed_t<int32_t, 16>, fixed_t<int16_t, 0>>, fixed_t<int64_t, 16>>);
static_assert(std::same_as<fixed::wider_t<fixed_t<int32_t, 16>, fixed_t<uint16_t, 1>>, fixed_t<int64_t, 17>>);
static_assert(std::same_as<fixed::wider_t<fixed_t<uint32_t, 16>, fixed_t<int16_t, 15>>, fixed_t<int64_t, 31>>);
static_assert(std::same_as<fixed::wider_t<fixed_t<uint32_t, 16>, fixed_t<uint16_t, 16>>, fixed_t<uint64_t, 32>>);

// ====================================================================================================================
// Construction
// ====================================================================================================================

namespace construction {

static_assert(sut_t{}.value == 0);
static_assert(sut_t{0}.value == 0);
static_assert(sut_t{1}.value == 1);
static_assert(sut_t{0xF1234}.value == 0xF1234);

} // namespace construction

// ====================================================================================================================
// Conversions
// ====================================================================================================================

namespace conversions {

// --------------------------------------------------------------------------------------------------------------------
// Precision and Size Conversions
// --------------------------------------------------------------------------------------------------------------------

template <typename from_t, typename to_t> struct conversion_test_t
{
    static constexpr auto delta = to_t::frac_bits - from_t::frac_bits;

    constexpr auto test() const noexcept -> void
    {
        if constexpr (delta > 0)
        {
            // increasing precision: raw value shifts left by delta
            static_assert(to_t{from_t{10}}.value == static_cast<typename to_t::value_t>(10) << delta);
        }
        else if constexpr (delta < 0)
        {
            // decreasing precision: start with pre-shifted value so result is exact
            constexpr auto from_value = static_cast<typename from_t::value_t>(10 << (-delta));
            static_assert(to_t{from_t{from_value}}.value == 10);
        }
        else
        {
            // same precision: value is just cast
            static_assert(to_t{from_t{10}}.value == 10);
        }
    }
};

// same precision, different size
template struct conversion_test_t<fixed_t<int8_t, 5>, fixed_t<int16_t, 5>>;
template struct conversion_test_t<fixed_t<int16_t, 5>, fixed_t<int8_t, 5>>;

// same size, different precision
template struct conversion_test_t<fixed_t<int8_t, 5>, fixed_t<int8_t, 7>>;
template struct conversion_test_t<fixed_t<int8_t, 7>, fixed_t<int8_t, 5>>;

// both change: widen + increase, widen + decrease, narrow + increase, narrow + decrease
template struct conversion_test_t<fixed_t<int8_t, 5>, fixed_t<int16_t, 7>>;
template struct conversion_test_t<fixed_t<int8_t, 7>, fixed_t<int16_t, 5>>;
template struct conversion_test_t<fixed_t<int16_t, 5>, fixed_t<int8_t, 7>>;
template struct conversion_test_t<fixed_t<int16_t, 7>, fixed_t<int8_t, 5>>;

// --------------------------------------------------------------------------------------------------------------------
// Wider-Range Conversions
// --------------------------------------------------------------------------------------------------------------------

// increase precision and widen: 64 fits in int8_t, but 64 << 2 = 256 does not
static_assert(fixed_t<int16_t, 9>{fixed_t<int8_t, 7>{64}}.value == 256);

// decrease precision and narrow: 256 fits in int16_t, but must widen before narrowing to get exact result
static_assert(fixed_t<int8_t, 7>{fixed_t<int16_t, 9>{256}}.value == 64);

// --------------------------------------------------------------------------------------------------------------------
// Rounding
// --------------------------------------------------------------------------------------------------------------------

namespace rounding {

using src_t        = fixed_t<int16_t, 4>;
using dst_t        = fixed_t<int8_t, 2>;
constexpr auto min = crv::min<int8_t>();
constexpr auto max = crv::max<int8_t>();

// default rm is truncate
static_assert(dst_t{src_t{min}}.value == min / 4);
static_assert(dst_t{src_t{min + 1}}.value == min / 4);
static_assert(dst_t{src_t{min + 3}}.value == min / 4);
static_assert(dst_t{src_t{min + 4}}.value == min / 4 + 1);
static_assert(dst_t{src_t{max - 4}}.value == max / 4 - 1);
static_assert(dst_t{src_t{max - 3}}.value == max / 4);
static_assert(dst_t{src_t{max}}.value == max / 4);

// explicitly use rne
constexpr auto rm = rounding_modes::shr::nearest_even;
static_assert(dst_t{src_t{min}, rm}.value == min / 4);
static_assert(dst_t{src_t{min + 1}, rm}.value == min / 4);
static_assert(dst_t{src_t{min + 3}, rm}.value == min / 4 + 1);
static_assert(dst_t{src_t{min + 4}, rm}.value == min / 4 + 1);
static_assert(dst_t{src_t{max - 4}, rm}.value == max / 4);
static_assert(dst_t{src_t{max - 3}, rm}.value == max / 4);
static_assert(dst_t{src_t{max}, rm}.value == max / 4 + 1);

} // namespace rounding

// --------------------------------------------------------------------------------------------------------------------
// Bool
// --------------------------------------------------------------------------------------------------------------------

static_assert(!fixed_t<int8_t, 5>{0});
static_assert(fixed_t<int8_t, 5>{max<int8_t>()});

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

static_assert(+sut_t{10}.value == 10);
static_assert(+sut_t{-10}.value == -10);

static_assert(-sut_t{10}.value == -10);
static_assert(-sut_t{-10}.value == 10);

} // namespace unary_arithmetic

// ====================================================================================================================
// Scalar Arithmetic
// ====================================================================================================================

namespace scalar_arithmetic {

// --------------------------------------------------------------------------------------------------------------------
// Addition
// --------------------------------------------------------------------------------------------------------------------

static_assert(sut_t{10} + 3 == sut_t{13});
static_assert(sut_t{-10} + 3 == sut_t{-7});
static_assert(3 + sut_t{10} == sut_t{13});
static_assert(3 + sut_t{-10} == sut_t{-7});

// --------------------------------------------------------------------------------------------------------------------
// Subtraction
// --------------------------------------------------------------------------------------------------------------------

static_assert(sut_t{10} - 3 == sut_t{7});
static_assert(sut_t{-10} - 3 == sut_t{-13});
static_assert(10 - sut_t{3} == sut_t{7});
static_assert(10 - sut_t{-3} == sut_t{13});

// subtraction is non-commutative
static_assert(sut_t{10} - 3 != 3 - sut_t{10});

// --------------------------------------------------------------------------------------------------------------------
// Multiplication
// --------------------------------------------------------------------------------------------------------------------

static_assert(sut_t{10} * 3 == sut_t{30});
static_assert(sut_t{-10} * 3 == sut_t{-30});
static_assert(3 * sut_t{10} == sut_t{30});
static_assert(3 * sut_t{-10} == sut_t{-30});

// identity and zero
static_assert(sut_t{7} * 1 == sut_t{7});
static_assert(sut_t{7} * 0 == sut_t{0});

// --------------------------------------------------------------------------------------------------------------------
// Division
// --------------------------------------------------------------------------------------------------------------------

static_assert(sut_t{30} / 3 == sut_t{10});
static_assert(sut_t{-30} / 3 == sut_t{-10});
static_assert(30 / sut_t{3} == sut_t{10});
static_assert(30 / sut_t{-3} == sut_t{-10});

// truncation
static_assert(sut_t{7} / 2 == sut_t{3});
static_assert(sut_t{-7} / 2 == sut_t{-3});

// division is non-commutative
static_assert(sut_t{12} / 3 != 3 / sut_t{12});

// --------------------------------------------------------------------------------------------------------------------
// Compound Assignment
// --------------------------------------------------------------------------------------------------------------------

struct scalar_compound_assignment_t : Test
{
    sut_t sut{10};
};

TEST_F(scalar_compound_assignment_t, addition)
{
    EXPECT_EQ(&sut, &(sut += 3));
    EXPECT_EQ(sut_t{13}, sut);
}

TEST_F(scalar_compound_assignment_t, subtraction)
{
    EXPECT_EQ(&sut, &(sut -= 3));
    EXPECT_EQ(sut_t{7}, sut);
}

TEST_F(scalar_compound_assignment_t, multiplication)
{
    EXPECT_EQ(&sut, &(sut *= 3));
    EXPECT_EQ(sut_t{30}, sut);
}

TEST_F(scalar_compound_assignment_t, division)
{
    EXPECT_EQ(&sut, &(sut /= 2));
    EXPECT_EQ(sut_t{5}, sut);
}

} // namespace scalar_arithmetic

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

// --------------------------------------------------------------------------------------------------------------------
// Subtraction
// --------------------------------------------------------------------------------------------------------------------

static_assert(sut_t{3} - sut_t{7} == sut_t{-4});
static_assert(sut_t{-3} - sut_t{7} == sut_t{-10});
static_assert(sut_t{3} - sut_t{-7} == sut_t{10});
static_assert(sut_t{-3} - sut_t{-7} == sut_t{4});

// --------------------------------------------------------------------------------------------------------------------
// Exact Multiplication
// --------------------------------------------------------------------------------------------------------------------

// mixed types, zeros
static_assert(typed_equal<fixed_t<int32_t, 8>>(multiply(fixed_t<int8_t, 3>{-11 << 3}, fixed_t<int16_t, 5>{0}),
                                               fixed_t<int32_t, 8>{0}));
static_assert(typed_equal<fixed_t<int32_t, 8>>(multiply(fixed_t<int8_t, 3>{0}, fixed_t<int16_t, 5>{-13 << 5}),
                                               fixed_t<int32_t, 8>{0}));
static_assert(typed_equal<fixed_t<int32_t, 8>>(multiply(fixed_t<int8_t, 3>{0}, fixed_t<int16_t, 5>{0}),
                                               fixed_t<int32_t, 8>{0}));
static_assert(typed_equal<fixed_t<int32_t, 8>>(multiply(fixed_t<int8_t, 3>{0}, fixed_t<int16_t, 5>{13 << 5}),
                                               fixed_t<int32_t, 8>{0}));
static_assert(typed_equal<fixed_t<int32_t, 8>>(multiply(fixed_t<int8_t, 3>{11 << 3}, fixed_t<int16_t, 5>{0}),
                                               fixed_t<int32_t, 8>{0}));

// mixed types, signed and unsigned
static_assert(typed_equal<fixed_t<int32_t, 8>>(multiply(fixed_t<int8_t, 3>{11 << 3}, fixed_t<int16_t, 5>{13 << 5}),
                                               fixed_t<int32_t, 8>{(11 * 13) << 8}));
static_assert(typed_equal<fixed_t<int32_t, 8>>(multiply(fixed_t<int8_t, 3>{11 << 3}, fixed_t<uint16_t, 5>{13 << 5}),
                                               fixed_t<int32_t, 8>{(11 * 13) << 8}));
static_assert(typed_equal<fixed_t<int32_t, 8>>(multiply(fixed_t<uint8_t, 3>{11 << 3}, fixed_t<int16_t, 5>{13 << 5}),
                                               fixed_t<int32_t, 8>{(11 * 13) << 8}));
static_assert(typed_equal<fixed_t<uint32_t, 8>>(multiply(fixed_t<uint8_t, 3>{11 << 3}, fixed_t<uint16_t, 5>{13 << 5}),
                                                fixed_t<uint32_t, 8>{(11 * 13) << 8}));

// mixed types with 128-bit results
static_assert(typed_equal<fixed_t<int128_t, 8>>(multiply(fixed_t<int8_t, 3>{11 << 3}, fixed_t<uint64_t, 5>{13 << 5}),
                                                fixed_t<int128_t, 8>{(11 * 13) << 8}));
static_assert(typed_equal<fixed_t<uint128_t, 8>>(multiply(fixed_t<uint8_t, 3>{11 << 3}, fixed_t<uint64_t, 5>{13 << 5}),
                                                 fixed_t<uint128_t, 8>{(11 * 13) << 8}));

// mixed signs
static_assert(typed_equal<fixed_t<int32_t, 8>>(multiply(fixed_t<int8_t, 3>{-11 << 3}, fixed_t<uint16_t, 5>{13 << 5}),
                                               fixed_t<int32_t, 8>{-(11 * 13) << 8}));
static_assert(typed_equal<fixed_t<int32_t, 8>>(multiply(fixed_t<int8_t, 3>{-11 << 3}, fixed_t<int16_t, 5>{-13 << 5}),
                                               fixed_t<int32_t, 8>{(11 * 13) << 8}));

// pure integer parts
static_assert(typed_equal<fixed_t<int64_t, 0>>(multiply(fixed_t<int16_t, 0>{7}, fixed_t<int32_t, 0>{11}),
                                               fixed_t<int64_t, 0>{77}));

// range limits
static_assert(typed_equal<fixed_t<int16_t, 14>>(multiply(fixed_t<int8_t, 7>{min<int8_t>()},
                                                         fixed_t<int8_t, 7>{min<int8_t>()}),
                                                fixed_t<int16_t, 14>{min<int8_t>() * min<int8_t>()}));
static_assert(typed_equal<fixed_t<int16_t, 14>>(multiply(fixed_t<int8_t, 7>{min<int8_t>()},
                                                         fixed_t<int8_t, 7>{max<int8_t>()}),
                                                fixed_t<int16_t, 14>{min<int8_t>() * max<int8_t>()}));
static_assert(typed_equal<fixed_t<int16_t, 14>>(multiply(fixed_t<int8_t, 7>{max<int8_t>()},
                                                         fixed_t<int8_t, 7>{min<int8_t>()}),
                                                fixed_t<int16_t, 14>{max<int8_t>() * min<int8_t>()}));
static_assert(typed_equal<fixed_t<int16_t, 14>>(multiply(fixed_t<int8_t, 7>{max<int8_t>()},
                                                         fixed_t<int8_t, 7>{max<int8_t>()}),
                                                fixed_t<int16_t, 14>{max<int8_t>() * max<int8_t>()}));

// 128-bit limits
static_assert(typed_equal<fixed_t<int128_t, 0>>(multiply(fixed_t<int64_t, 0>{max<int64_t>()},
                                                         fixed_t<int64_t, 0>{max<int64_t>()}),
                                                fixed_t<int128_t, 0>{int128_t{max<int64_t>()} * max<int64_t>()}));
static_assert(typed_equal<fixed_t<uint128_t, 0>>(multiply(fixed_t<uint64_t, 0>{max<uint64_t>()},
                                                          fixed_t<uint64_t, 0>{max<uint64_t>()}),
                                                 fixed_t<uint128_t, 0>{uint128_t{max<uint64_t>()} * max<uint64_t>()}));
static_assert(typed_equal<fixed_t<int128_t, 126>>(multiply(fixed_t<int64_t, 63>{max<int64_t>()},
                                                           fixed_t<int64_t, 63>{max<int64_t>()}),
                                                  fixed_t<int128_t, 126>{int128_t{max<int64_t>()} * max<int64_t>()}));
static_assert(typed_equal<fixed_t<uint128_t, 128>>(
    multiply(fixed_t<uint64_t, 64>{max<uint64_t>()}, fixed_t<uint64_t, 64>{max<uint64_t>()}),
    fixed_t<uint128_t, 128>{uint128_t{max<uint64_t>()} * max<uint64_t>()}));

// --------------------------------------------------------------------------------------------------------------------
// Multiplication to Specific Type with Rounding Mode
// --------------------------------------------------------------------------------------------------------------------

static_assert(typed_equal<fixed_t<int8_t, 1>>(multiply<fixed_t<int8_t, 1>>(fixed_t<int16_t, 1>{2 << 1},
                                                                           fixed_t<int32_t, 1>{3 << 1},
                                                                           rounding_modes::shr::truncate),
                                              fixed_t<int8_t, 1>{(2 * 3) << 1}));
static_assert(typed_equal<fixed_t<int8_t, 1>>(multiply<fixed_t<int8_t, 1>>(fixed_t<int16_t, 1>{2 << 1},
                                                                           fixed_t<uint32_t, 1>{3 << 1},
                                                                           rounding_modes::shr::truncate),
                                              fixed_t<int8_t, 1>{(2 * 3) << 1}));
static_assert(typed_equal<fixed_t<int8_t, 1>>(multiply<fixed_t<int8_t, 1>>(fixed_t<uint16_t, 1>{2 << 1},
                                                                           fixed_t<int32_t, 1>{3 << 1},
                                                                           rounding_modes::shr::truncate),
                                              fixed_t<int8_t, 1>{(2 * 3) << 1}));
static_assert(typed_equal<fixed_t<int8_t, 1>>(multiply<fixed_t<int8_t, 1>>(fixed_t<uint16_t, 1>{2 << 1},
                                                                           fixed_t<uint32_t, 1>{3 << 1},
                                                                           rounding_modes::shr::truncate),
                                              fixed_t<int8_t, 1>{(2 * 3) << 1}));

TEST_F(fixed_test_with_rounding_mode_t, multiplication_to_specific_type)
{
    using out_t              = fixed_t<uint_t, 1>;
    auto const lhs           = fixed_t<uint8_t, 1>(2 << 1);
    auto const rhs           = fixed_t<int16_t, 1>(3 << 1);
    auto const expected_bias = uint32_t{23};
    auto const expected      = out_t{29};
    EXPECT_CALL(mock_rounding_mode, bias((2 * 3) << 2, 1)).WillOnce(Return(expected_bias));
    EXPECT_CALL(mock_rounding_mode, carry(expected_bias >> 1, expected_bias, 1)).WillOnce(Return(expected.value));

    auto const actual = multiply<out_t>(lhs, rhs, rounding_mode);

    EXPECT_EQ(expected, actual);
}

// --------------------------------------------------------------------------------------------------------------------
// Multiplication to LHS Type with Rounding Mode
// --------------------------------------------------------------------------------------------------------------------

static_assert(typed_equal<fixed_t<int8_t, 1>>(multiply(fixed_t<int8_t, 1>{2 << 1}, fixed_t<int8_t, 1>{3 << 1},
                                                       rounding_modes::shr::truncate),
                                              fixed_t<int8_t, 1>{(2 * 3) << 1}));
static_assert(typed_equal<fixed_t<int8_t, 1>>(multiply(fixed_t<int8_t, 1>{2 << 1}, fixed_t<uint8_t, 1>{3 << 1},
                                                       rounding_modes::shr::truncate),
                                              fixed_t<int8_t, 1>{(2 * 3) << 1}));
static_assert(typed_equal<fixed_t<uint8_t, 1>>(multiply(fixed_t<uint8_t, 1>{2 << 1}, fixed_t<int8_t, 1>{3 << 1},
                                                        rounding_modes::shr::truncate),
                                               fixed_t<uint8_t, 1>{(2 * 3) << 1}));
static_assert(typed_equal<fixed_t<uint8_t, 1>>(multiply(fixed_t<uint8_t, 1>{2 << 1}, fixed_t<uint8_t, 1>{3 << 1},
                                                        rounding_modes::shr::truncate),
                                               fixed_t<uint8_t, 1>{(2 * 3) << 1}));

TEST_F(fixed_test_with_rounding_mode_t, multiplication_to_lhs_type)
{
    using sut_t              = fixed_t<int_t, 1>;
    auto const lhs           = sut_t{2 << 1};
    auto const rhs           = sut_t{3 << 1};
    auto const expected_bias = sut_t::value_t{23};
    auto const expected      = sut_t{29};
    EXPECT_CALL(mock_rounding_mode, bias((2 * 3) << 2, 1)).WillOnce(Return(expected_bias));
    EXPECT_CALL(mock_rounding_mode, carry(expected_bias >> 1, expected_bias, 1)).WillOnce(Return(expected.value));

    auto const actual = multiply(lhs, rhs, rounding_mode);

    EXPECT_EQ(expected, actual);
}

// --------------------------------------------------------------------------------------------------------------------
// Division
// --------------------------------------------------------------------------------------------------------------------

namespace division {

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
        auto const lhs      = fixed_t<uint64_t, vector_t::lhs_frac_bits>{vector.lhs};
        auto const rhs      = fixed_t<uint64_t, vector_t::rhs_frac_bits>{vector.rhs};
        auto const expected = fixed_t<uint64_t, vector_t::out_frac_bits>{vector.expected};

        auto const actual = divide<vector_t::out_frac_bits>(lhs, rhs);

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
    {"0/1", 0 << lhs_frac_bits, 1 << rhs_frac_bits, (0 << out_frac_bits) / 1 + 0},
    {"1/1", 1 << lhs_frac_bits, 1 << rhs_frac_bits, (1 << out_frac_bits) / 1 + 0},
    {"2/1", 2 << lhs_frac_bits, 1 << rhs_frac_bits, (2 << out_frac_bits) / 1 + 0},
    {"0/2", 0 << lhs_frac_bits, 2 << rhs_frac_bits, (0 << out_frac_bits) / 2 + 0},
    {"1/2", 1 << lhs_frac_bits, 2 << rhs_frac_bits, (1 << out_frac_bits) / 2 + 0},
    {"2/2", 2 << lhs_frac_bits, 2 << rhs_frac_bits, (2 << out_frac_bits) / 2 + 0},
    {"3/2", 3 << lhs_frac_bits, 2 << rhs_frac_bits, (3 << out_frac_bits) / 2 + 0},
    {"0/3", 0 << lhs_frac_bits, 3 << rhs_frac_bits, (0 << out_frac_bits) / 3 + 0},
    {"1/3", 1 << lhs_frac_bits, 3 << rhs_frac_bits, (1 << out_frac_bits) / 3 + 0},
    {"2/3", 2 << lhs_frac_bits, 3 << rhs_frac_bits, (2 << out_frac_bits) / 3 + 1},
    {"3/3", 3 << lhs_frac_bits, 3 << rhs_frac_bits, (3 << out_frac_bits) / 3 + 0},
    {"4/3", 4 << lhs_frac_bits, 3 << rhs_frac_bits, (4 << out_frac_bits) / 3 + 0},
    {"0/4", 0 << lhs_frac_bits, 4 << rhs_frac_bits, (0 << out_frac_bits) / 4 + 0},
    {"1/4", 1 << lhs_frac_bits, 4 << rhs_frac_bits, (1 << out_frac_bits) / 4 + 0},
    {"2/4", 2 << lhs_frac_bits, 4 << rhs_frac_bits, (2 << out_frac_bits) / 4 + 0},
    {"3/4", 3 << lhs_frac_bits, 4 << rhs_frac_bits, (3 << out_frac_bits) / 4 + 0},
    {"4/4", 4 << lhs_frac_bits, 4 << rhs_frac_bits, (4 << out_frac_bits) / 4 + 0},
    {"5/4", 5 << lhs_frac_bits, 4 << rhs_frac_bits, (5 << out_frac_bits) / 4 + 0},
    {"0/5", 0 << lhs_frac_bits, 5 << rhs_frac_bits, (0 << out_frac_bits) / 5 + 0},
    {"1/5", 1 << lhs_frac_bits, 5 << rhs_frac_bits, (1 << out_frac_bits) / 5 + 0},
    {"2/5", 2 << lhs_frac_bits, 5 << rhs_frac_bits, (2 << out_frac_bits) / 5 + 0},
    {"3/5", 3 << lhs_frac_bits, 5 << rhs_frac_bits, (3 << out_frac_bits) / 5 + 1},
    {"4/5", 4 << lhs_frac_bits, 5 << rhs_frac_bits, (4 << out_frac_bits) / 5 + 1},
    {"5/5", 5 << lhs_frac_bits, 5 << rhs_frac_bits, (5 << out_frac_bits) / 5 + 0},
    {"6/5", 6 << lhs_frac_bits, 5 << rhs_frac_bits, (6 << out_frac_bits) / 5 + 0},
};
INSTANTIATE_TEST_SUITE_P(cases, fixed_division_vector_test_t, ValuesIn(vectors),
                         test_name_generator_t<specialized_vector_t>{});

} // namespace division

// --------------------------------------------------------------------------------------------------------------------
// Compound Assignment
// --------------------------------------------------------------------------------------------------------------------

struct fixed_test_compound_assignment_t : Test
{
    static constexpr auto lhs_value = 3;
    static constexpr auto rhs_value = 7;

    sut_t lhs{lhs_value << frac_bits};
    sut_t rhs{rhs_value << frac_bits};
};

TEST_F(fixed_test_compound_assignment_t, addition)
{
    EXPECT_EQ(&lhs, &(lhs += rhs));
}

TEST_F(fixed_test_compound_assignment_t, subtraction)
{
    EXPECT_EQ(&lhs, &(lhs -= rhs));
}

TEST_F(fixed_test_compound_assignment_t, multiplication)
{
    auto const expected_product = lhs_value * rhs_value << frac_bits;
    EXPECT_EQ(&lhs, &(lhs *= rhs));
    EXPECT_EQ(expected_product, lhs.value);
}

} // namespace fixed_arithmetic

// ====================================================================================================================
// Math Functions
// ====================================================================================================================

namespace math_functions {

static_assert(abs(fixed_t<int_t, 3>{-max<int_t>()}).value == fixed_t<int_t, 3>{max<int_t>()}.value);
static_assert(abs(fixed_t<int_t, 3>{-1}).value == fixed_t<int_t, 3>{1}.value);
static_assert(abs(fixed_t<int_t, 3>{0}).value == fixed_t<int_t, 3>{0}.value);
static_assert(abs(fixed_t<int_t, 3>{1}).value == fixed_t<int_t, 3>{1}.value);
static_assert(abs(fixed_t<int_t, 3>{max<int_t>()}).value == fixed_t<int_t, 3>{max<int_t>()}.value);

static_assert(abs(fixed_t<uint_t, 3>{0}).value == fixed_t<uint_t, 3>{0}.value);
static_assert(abs(fixed_t<uint_t, 3>{1}).value == fixed_t<uint_t, 3>{1}.value);
static_assert(abs(fixed_t<uint_t, 3>{max<int_t>()}).value == fixed_t<uint_t, 3>{max<int_t>()}.value);

} // namespace math_functions

} // namespace
} // namespace crv
