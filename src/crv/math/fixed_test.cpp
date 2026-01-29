// SPDX-License-Identifier: MIT
/**
    \file
    \copyright Copyright (C) 2026 Frank Secilia
*/

#include <crv/math/fixed.hpp>
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <crv/test/typed_equal.hpp>

namespace crv {
namespace {

using value_t            = int_t;
constexpr auto frac_bits = 21;
using sut_t              = fixed_t<value_t, frac_bits>;

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

static_assert(sut_t{3} + sut_t{7} == sut_t{10});
static_assert(sut_t{-3} + sut_t{7} == sut_t{4});
static_assert(sut_t{3} + sut_t{-7} == sut_t{-4});
static_assert(sut_t{-3} + sut_t{-7} == sut_t{-10});

static_assert(sut_t{3} - sut_t{7} == sut_t{-4});
static_assert(sut_t{-3} - sut_t{7} == sut_t{-10});
static_assert(sut_t{3} - sut_t{-7} == sut_t{10});
static_assert(sut_t{-3} - sut_t{-7} == sut_t{4});

// mixed types, zeros
static_assert(typed_equal<fixed_t<int32_t, 3 + 5>>(fixed_t<int8_t, 3>{-11 << 3} * fixed_t<int16_t, 5>{0},
                                                   fixed_t<int32_t, 8>{0}),
              "fixed_t: mixed negative*0 failed");
static_assert(typed_equal<fixed_t<int32_t, 3 + 5>>(fixed_t<int8_t, 3>{0} * fixed_t<int16_t, 5>{-13 << 5},
                                                   fixed_t<int32_t, 8>{0}),
              "fixed_t: mixed 0*negative failed");
static_assert(typed_equal<fixed_t<int32_t, 3 + 5>>(fixed_t<int8_t, 3>{0} * fixed_t<int16_t, 5>{0},
                                                   fixed_t<int32_t, 8>{0}),
              "fixed_t: mixed 0*0 failed");
static_assert(typed_equal<fixed_t<int32_t, 3 + 5>>(fixed_t<int8_t, 3>{0} * fixed_t<int16_t, 5>{13 << 5},
                                                   fixed_t<int32_t, 8>{0}),
              "fixed_t: mixed 0*positive failed");
static_assert(typed_equal<fixed_t<int32_t, 3 + 5>>(fixed_t<int8_t, 3>{11 << 3} * fixed_t<int16_t, 5>{0},
                                                   fixed_t<int32_t, 8>{0}),
              "fixed_t: mixed positive*0 failed");

// mixed types, signed and unsigned
static_assert(typed_equal<fixed_t<int32_t, 3 + 5>>(fixed_t<int8_t, 3>{11 << 3} * fixed_t<int16_t, 5>{13 << 5},
                                                   fixed_t<int32_t, 8>{(11 * 13) << 8}),
              "fixed_t: mixed int*int failed");
static_assert(typed_equal<fixed_t<int32_t, 3 + 5>>(fixed_t<int8_t, 3>{11 << 3} * fixed_t<uint16_t, 5>{13 << 5},
                                                   fixed_t<int32_t, 8>{(11 * 13) << 8}),
              "fixed_t: mixed int*uint failed");
static_assert(typed_equal<fixed_t<int32_t, 3 + 5>>(fixed_t<uint8_t, 3>{11 << 3} * fixed_t<int16_t, 5>{13 << 5},
                                                   fixed_t<int32_t, 8>{(11 * 13) << 8}),
              "fixed_t: mixed uint*int failed");
static_assert(typed_equal<fixed_t<uint32_t, 3 + 5>>(fixed_t<uint8_t, 3>{11 << 3} * fixed_t<uint16_t, 5>{13 << 5},
                                                    fixed_t<uint32_t, 8>{(11 * 13) << 8}),
              "fixed_t: mixed uint*uint failed");

// mixed types with 128-bit results
static_assert(typed_equal<fixed_t<int128_t, 3 + 5>>(fixed_t<int8_t, 3>{11 << 3} * fixed_t<uint64_t, 5>{13 << 5},
                                                    fixed_t<int128_t, 8>{(11 * 13) << 8}),
              "fixed_t: mixed int8*uint64 failed");
static_assert(typed_equal<fixed_t<uint128_t, 3 + 5>>(fixed_t<uint8_t, 3>{11 << 3} * fixed_t<uint64_t, 5>{13 << 5},
                                                     fixed_t<uint128_t, 8>{(11 * 13) << 8}),
              "fixed_t: mixed uint8*uint64 failed");

// mixed signs
static_assert(typed_equal<fixed_t<int32_t, 3 + 5>>(fixed_t<int8_t, 3>{-11 << 3} * fixed_t<uint16_t, 5>{13 << 5},
                                                   fixed_t<int32_t, 8>{-(11 * 13) << 8}),
              "fixed_t: mixed negative*positive failed");

// double negative
static_assert(typed_equal<fixed_t<int32_t, 3 + 5>>(fixed_t<int8_t, 3>{-11 << 3} * fixed_t<int16_t, 5>{-13 << 5},
                                                   fixed_t<int32_t, 8>{(11 * 13) << 8}),
              "fixed_t: mixed negative*negative failed");

// pure integer parts
static_assert(typed_equal<fixed_t<int64_t, 0>>(fixed_t<int16_t, 0>{7} * fixed_t<int32_t, 0>{11},
                                               fixed_t<int64_t, 0>{77}),
              "fixed_t: integer*integer failed");

// range limits
static_assert(typed_equal<fixed_t<int16_t, 7 + 7>>(fixed_t<int8_t, 7>{min<int8_t>()}
                                                       * fixed_t<int8_t, 7>{min<int8_t>()},
                                                   fixed_t<int16_t, 14>{min<int8_t>() * min<int8_t>()}),
              "fixed_t: min*min failed");
static_assert(typed_equal<fixed_t<int16_t, 7 + 7>>(fixed_t<int8_t, 7>{min<int8_t>()}
                                                       * fixed_t<int8_t, 7>{max<int8_t>()},
                                                   fixed_t<int16_t, 14>{min<int8_t>() * max<int8_t>()}),
              "fixed_t: min*max failed");
static_assert(typed_equal<fixed_t<int16_t, 7 + 7>>(fixed_t<int8_t, 7>{max<int8_t>()}
                                                       * fixed_t<int8_t, 7>{min<int8_t>()},
                                                   fixed_t<int16_t, 14>{max<int8_t>() * min<int8_t>()}),
              "fixed_t: max*min failed");
static_assert(typed_equal<fixed_t<int16_t, 7 + 7>>(fixed_t<int8_t, 7>{max<int8_t>()}
                                                       * fixed_t<int8_t, 7>{max<int8_t>()},
                                                   fixed_t<int16_t, 14>{max<int8_t>() * max<int8_t>()}),
              "fixed_t: max*max failed");

// 128-bit limits
static_assert(typed_equal<fixed_t<int128_t, 0>>(fixed_t<int64_t, 0>{max<int64_t>()}
                                                    * fixed_t<int64_t, 0>{max<int64_t>()},
                                                fixed_t<int128_t, 0>{int128_t{max<int64_t>()} * max<int64_t>()}),
              "fixed_t: max signed integer*integer failed");
static_assert(typed_equal<fixed_t<uint128_t, 0>>(fixed_t<uint64_t, 0>{max<uint64_t>()}
                                                     * fixed_t<uint64_t, 0>{max<uint64_t>()},
                                                 fixed_t<uint128_t, 0>{uint128_t{max<uint64_t>()} * max<uint64_t>()}),
              "fixed_t: max unsigned integer*integer failed");
static_assert(typed_equal<fixed_t<int128_t, 126>>(fixed_t<int64_t, 63>{max<int64_t>()}
                                                      * fixed_t<int64_t, 63>{max<int64_t>()},
                                                  fixed_t<int128_t, 126>{int128_t{max<int64_t>()} * max<int64_t>()}),
              "fixed_t: max signed fraction*fraction failed");
static_assert(typed_equal<fixed_t<uint128_t, 128>>(
                  fixed_t<uint64_t, 64>{max<uint64_t>()} * fixed_t<uint64_t, 64>{max<uint64_t>()},
                  fixed_t<uint128_t, 128>{uint128_t{max<uint64_t>()} * max<uint64_t>()}),
              "fixed_t: max unsigned fraction*fraction failed");

struct fixed_test_compound_assignment_t : Test
{
    static constexpr auto lhs_value = 3;
    static constexpr auto rhs_value = 7;

    sut_t lhs{lhs_value << frac_bits};
    sut_t rhs{rhs_value << frac_bits};
};

TEST_F(fixed_test_compound_assignment_t, addition)
{
    ASSERT_EQ(&lhs, &(lhs += rhs));
}

TEST_F(fixed_test_compound_assignment_t, subtraction)
{
    ASSERT_EQ(&lhs, &(lhs -= rhs));
}

TEST_F(fixed_test_compound_assignment_t, multiplication)
{
    auto const expected_product = lhs_value * rhs_value << frac_bits;
    ASSERT_EQ(&lhs, &(lhs *= rhs));
    ASSERT_EQ(expected_product, lhs.value);
}

} // namespace binary_arithmetic

} // namespace
} // namespace crv
