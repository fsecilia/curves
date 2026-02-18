// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include <crv/math/fixed/fixed.hpp>
#include <crv/math/io.hpp>
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <crv/test/typed_equal.hpp>
#include <concepts>

namespace crv {
namespace {

using value_t            = int_t;
constexpr auto frac_bits = 21;
using sut_t              = fixed_t<value_t, frac_bits>;

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

// zero initialization works
static_assert(sut_t{}.value == 0, "fixed_t: zero initialization failed");

// zero is always zero; there is no offset
static_assert(sut_t{0}.value == 0, "fixed_t: value initialization tranlsated value");

// value initialization is direct; no rescaling is performed
static_assert(sut_t{1}.value == 1, "fixed_t: value initialization scaled value");

// 0 and 1 are not special
static_assert(sut_t{0xF1234}.value == 0xF1234, "fixed_t: value initialization failed");

} // namespace construction

// ====================================================================================================================
// Conversions
// ====================================================================================================================

namespace conversions {

// --------------------------------------------------------------------------------------------------------------------
// Size Conversions
// --------------------------------------------------------------------------------------------------------------------

// widen type
static_assert(fixed_t<int16_t, 5>{fixed_t<int8_t, 5>{10}}.value == 10, "fixed_t: widen type failed");

// narrow type
static_assert(fixed_t<int8_t, 5>{fixed_t<int16_t, 5>{10}}.value == 10, "fixed_t: narrow type failed");

// --------------------------------------------------------------------------------------------------------------------
// Precision Conversions
// --------------------------------------------------------------------------------------------------------------------

// increase precision
static_assert(fixed_t<int8_t, 7>{fixed_t<int8_t, 5>{10}}.value == 40, "fixed_t: increase precision failed");

// decrease precision
static_assert(fixed_t<int8_t, 5>{fixed_t<int8_t, 7>{40}}.value == 10, "fixed_t: decrease precision failed");

// --------------------------------------------------------------------------------------------------------------------
// Size and Precision Conversions
// --------------------------------------------------------------------------------------------------------------------

// increase precision and widen type
static_assert(fixed_t<int16_t, 7>{fixed_t<int8_t, 5>{10}}.value == 40, "fixed_t: increase precision and widen failed");

// increase precision and widen type requiring conversion at wider range
static_assert(fixed_t<int16_t, 9>{fixed_t<int8_t, 7>{64}}.value == 256,
              "fixed_t: increase precision and widen early failed");

// increase precision and narrow type
static_assert(fixed_t<int8_t, 7>{fixed_t<int16_t, 5>{10}}.value == 40, "fixed_t: increase precision and narrow failed");

// decrease precision and widen type
static_assert(fixed_t<int16_t, 5>{fixed_t<int8_t, 7>{40}}.value == 10, "fixed_t: decrease precision and widen failed");

// decrease precision and narrow type
static_assert(fixed_t<int8_t, 5>{fixed_t<int16_t, 7>{40}}.value == 10, "fixed_t: decrease precision and narrow failed");

// decrease precision and narrow type requiring conversion at wider range
static_assert(fixed_t<int8_t, 7>{fixed_t<int16_t, 9>{256}}.value == 64,
              "fixed_t: decrease precision and narrow late failed");

// --------------------------------------------------------------------------------------------------------------------
// Rounding
// --------------------------------------------------------------------------------------------------------------------

static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{min<int8_t>()}}.value == min<int8_t>() / 4);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{min<int8_t>() + 1}}.value == min<int8_t>() / 4);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{min<int8_t>() + 2}}.value == min<int8_t>() / 4 + 1);

static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{-107}}.value == -27);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{-106}}.value == -26);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{-103}}.value == -26);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{-102}}.value == -25);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{-101}}.value == -25);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{-100}}.value == -25);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{-99}}.value == -25);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{-98}}.value == -24);

static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{-4}}.value == -1);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{-3}}.value == -1);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{-2}}.value == 0);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{-1}}.value == 0);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{0}}.value == 0);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{1}}.value == 0);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{2}}.value == 1);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{3}}.value == 1);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{4}}.value == 1);

static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{97}}.value == 24);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{98}}.value == 25);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{100}}.value == 25);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{101}}.value == 25);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{102}}.value == 26);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{103}}.value == 26);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{105}}.value == 26);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{106}}.value == 27);

static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{max<int8_t>() - 2}}.value == max<int8_t>() / 4);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{max<int8_t>() - 1}}.value == max<int8_t>() / 4 + 1);
static_assert(fixed_t<int8_t, 2>{fixed_t<int16_t, 4>{max<int8_t>()}}.value == max<int8_t>() / 4 + 1);

static_assert(fixed_t<uint8_t, 2>{fixed_t<uint16_t, 4>{0}}.value == 0);
static_assert(fixed_t<uint8_t, 2>{fixed_t<uint16_t, 4>{1}}.value == 0);
static_assert(fixed_t<uint8_t, 2>{fixed_t<uint16_t, 4>{2}}.value == 1);
static_assert(fixed_t<uint8_t, 2>{fixed_t<uint16_t, 4>{3}}.value == 1);
static_assert(fixed_t<uint8_t, 2>{fixed_t<uint16_t, 4>{4}}.value == 1);
static_assert(fixed_t<uint8_t, 2>{fixed_t<uint16_t, 4>{5}}.value == 1);
static_assert(fixed_t<uint8_t, 2>{fixed_t<uint16_t, 4>{6}}.value == 2);

static_assert(fixed_t<uint8_t, 2>{fixed_t<uint16_t, 4>{max<uint8_t>() - 2}}.value == max<uint8_t>() / 4);
static_assert(fixed_t<uint8_t, 2>{fixed_t<uint16_t, 4>{max<uint8_t>() - 1}}.value == max<uint8_t>() / 4 + 1);
static_assert(fixed_t<uint8_t, 2>{fixed_t<uint16_t, 4>{max<uint8_t>()}}.value == max<uint8_t>() / 4 + 1);

// --------------------------------------------------------------------------------------------------------------------
// Bool
// --------------------------------------------------------------------------------------------------------------------

static_assert(fixed_t<int8_t, 5>{min<int8_t>()});
static_assert(fixed_t<int8_t, 5>{min<int8_t>() + 1});
static_assert(fixed_t<int8_t, 5>{-1});
static_assert(!fixed_t<int8_t, 5>{0});
static_assert(fixed_t<int8_t, 5>{1});
static_assert(fixed_t<int8_t, 5>{max<int8_t>() - 1});
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
// Binary Arithmetic
// ====================================================================================================================

namespace binary_arithmetic {

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
// Multiplication
// --------------------------------------------------------------------------------------------------------------------

// mixed types, zeros
static_assert(typed_equal<fixed_t<int32_t, 3 + 5>>(multiply(fixed_t<int8_t, 3>{-11 << 3}, fixed_t<int16_t, 5>{0}),
                                                   fixed_t<int32_t, 8>{0}),
              "fixed_t: mixed negative*0 failed");
static_assert(typed_equal<fixed_t<int32_t, 3 + 5>>(multiply(fixed_t<int8_t, 3>{0}, fixed_t<int16_t, 5>{-13 << 5}),
                                                   fixed_t<int32_t, 8>{0}),
              "fixed_t: mixed 0*negative failed");
static_assert(typed_equal<fixed_t<int32_t, 3 + 5>>(multiply(fixed_t<int8_t, 3>{0}, fixed_t<int16_t, 5>{0}),
                                                   fixed_t<int32_t, 8>{0}),
              "fixed_t: mixed 0*0 failed");
static_assert(typed_equal<fixed_t<int32_t, 3 + 5>>(multiply(fixed_t<int8_t, 3>{0}, fixed_t<int16_t, 5>{13 << 5}),
                                                   fixed_t<int32_t, 8>{0}),
              "fixed_t: mixed 0*positive failed");
static_assert(typed_equal<fixed_t<int32_t, 3 + 5>>(multiply(fixed_t<int8_t, 3>{11 << 3}, fixed_t<int16_t, 5>{0}),
                                                   fixed_t<int32_t, 8>{0}),
              "fixed_t: mixed positive*0 failed");

// mixed types, signed and unsigned
static_assert(typed_equal<fixed_t<int32_t, 3 + 5>>(multiply(fixed_t<int8_t, 3>{11 << 3}, fixed_t<int16_t, 5>{13 << 5}),
                                                   fixed_t<int32_t, 8>{(11 * 13) << 8}),
              "fixed_t: mixed int*int failed");
static_assert(typed_equal<fixed_t<int32_t, 3 + 5>>(multiply(fixed_t<int8_t, 3>{11 << 3}, fixed_t<uint16_t, 5>{13 << 5}),
                                                   fixed_t<int32_t, 8>{(11 * 13) << 8}),
              "fixed_t: mixed int*uint failed");
static_assert(typed_equal<fixed_t<int32_t, 3 + 5>>(multiply(fixed_t<uint8_t, 3>{11 << 3}, fixed_t<int16_t, 5>{13 << 5}),
                                                   fixed_t<int32_t, 8>{(11 * 13) << 8}),
              "fixed_t: mixed uint*int failed");
static_assert(typed_equal<fixed_t<uint32_t, 3 + 5>>(multiply(fixed_t<uint8_t, 3>{11 << 3},
                                                             fixed_t<uint16_t, 5>{13 << 5}),
                                                    fixed_t<uint32_t, 8>{(11 * 13) << 8}),
              "fixed_t: mixed uint*uint failed");

// mixed types with 128-bit results
static_assert(typed_equal<fixed_t<int128_t, 3 + 5>>(multiply(fixed_t<int8_t, 3>{11 << 3},
                                                             fixed_t<uint64_t, 5>{13 << 5}),
                                                    fixed_t<int128_t, 8>{(11 * 13) << 8}),
              "fixed_t: mixed int8*uint64 failed");
static_assert(typed_equal<fixed_t<uint128_t, 3 + 5>>(multiply(fixed_t<uint8_t, 3>{11 << 3},
                                                              fixed_t<uint64_t, 5>{13 << 5}),
                                                     fixed_t<uint128_t, 8>{(11 * 13) << 8}),
              "fixed_t: mixed uint8*uint64 failed");

// mixed signs
static_assert(typed_equal<fixed_t<int32_t, 3 + 5>>(multiply(fixed_t<int8_t, 3>{-11 << 3},
                                                            fixed_t<uint16_t, 5>{13 << 5}),
                                                   fixed_t<int32_t, 8>{-(11 * 13) << 8}),
              "fixed_t: mixed negative*positive failed");

// double negative
static_assert(typed_equal<fixed_t<int32_t, 3 + 5>>(multiply(fixed_t<int8_t, 3>{-11 << 3},
                                                            fixed_t<int16_t, 5>{-13 << 5}),
                                                   fixed_t<int32_t, 8>{(11 * 13) << 8}),
              "fixed_t: mixed negative*negative failed");

// pure integer parts
static_assert(typed_equal<fixed_t<int64_t, 0>>(multiply(fixed_t<int16_t, 0>{7}, fixed_t<int32_t, 0>{11}),
                                               fixed_t<int64_t, 0>{77}),
              "fixed_t: integer*integer failed");

// range limits
static_assert(typed_equal<fixed_t<int16_t, 7 + 7>>(multiply(fixed_t<int8_t, 7>{min<int8_t>()},
                                                            fixed_t<int8_t, 7>{min<int8_t>()}),
                                                   fixed_t<int16_t, 14>{min<int8_t>() * min<int8_t>()}),
              "fixed_t: min*min failed");
static_assert(typed_equal<fixed_t<int16_t, 7 + 7>>(multiply(fixed_t<int8_t, 7>{min<int8_t>()},
                                                            fixed_t<int8_t, 7>{max<int8_t>()}),
                                                   fixed_t<int16_t, 14>{min<int8_t>() * max<int8_t>()}),
              "fixed_t: min*max failed");
static_assert(typed_equal<fixed_t<int16_t, 7 + 7>>(multiply(fixed_t<int8_t, 7>{max<int8_t>()},
                                                            fixed_t<int8_t, 7>{min<int8_t>()}),
                                                   fixed_t<int16_t, 14>{max<int8_t>() * min<int8_t>()}),
              "fixed_t: max*min failed");
static_assert(typed_equal<fixed_t<int16_t, 7 + 7>>(multiply(fixed_t<int8_t, 7>{max<int8_t>()},
                                                            fixed_t<int8_t, 7>{max<int8_t>()}),
                                                   fixed_t<int16_t, 14>{max<int8_t>() * max<int8_t>()}),
              "fixed_t: max*max failed");

// 128-bit limits
static_assert(typed_equal<fixed_t<int128_t, 0>>(multiply(fixed_t<int64_t, 0>{max<int64_t>()},
                                                         fixed_t<int64_t, 0>{max<int64_t>()}),
                                                fixed_t<int128_t, 0>{int128_t{max<int64_t>()} * max<int64_t>()}),
              "fixed_t: max signed integer*integer failed");
static_assert(typed_equal<fixed_t<uint128_t, 0>>(multiply(fixed_t<uint64_t, 0>{max<uint64_t>()},
                                                          fixed_t<uint64_t, 0>{max<uint64_t>()}),
                                                 fixed_t<uint128_t, 0>{uint128_t{max<uint64_t>()} * max<uint64_t>()}),
              "fixed_t: max unsigned integer*integer failed");
static_assert(typed_equal<fixed_t<int128_t, 126>>(multiply(fixed_t<int64_t, 63>{max<int64_t>()},
                                                           fixed_t<int64_t, 63>{max<int64_t>()}),
                                                  fixed_t<int128_t, 126>{int128_t{max<int64_t>()} * max<int64_t>()}),
              "fixed_t: max signed fraction*fraction failed");
static_assert(typed_equal<fixed_t<uint128_t, 128>>(
                  multiply(fixed_t<uint64_t, 64>{max<uint64_t>()}, fixed_t<uint64_t, 64>{max<uint64_t>()}),
                  fixed_t<uint128_t, 128>{uint128_t{max<uint64_t>()} * max<uint64_t>()}),
              "fixed_t: max unsigned fraction*fraction failed");

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
    // (2^64 - 1) / 1 = max<uint64_t>()
    test(vector_t<0, 0, 0>{"safe max", max<uint64_t>(), 1, max<uint64_t>()});

    // 2^64/2 = 2^63
    test(vector_t<60, 0, 0>{"valid high bit", 16, 2, 1ULL << 63});

    // 2^64/1 = 2^64
    test(vector_t<60, 0, 0>{"saturates 16 << 60", 16, 1, max<uint64_t>()});
    test(vector_t<64, 0, 0>{"saturates 1 << 64", 1, 1, max<uint64_t>()});
}

// --------------------------------------------------------------------------------------------------------------------

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
    // basics up to 5 to cover rounding
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

} // namespace binary_arithmetic

// ====================================================================================================================
// Math Functions
// ====================================================================================================================

namespace math_functions {

// --------------------------------------------------------------------------------------------------------------------
// abs
// --------------------------------------------------------------------------------------------------------------------

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
