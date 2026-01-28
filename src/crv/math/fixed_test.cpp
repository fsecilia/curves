// SPDX-License-Identifier: MIT
/**
  \file
  \copyright Copyright (C) 2026 Frank Secilia
*/

#include <crv/math/fixed.hpp>
#include <crv/test/test.hpp>

namespace crv {
namespace {

using value_t            = int;
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

// --------------------------------------------------------------------------------------------------------------------
// Bool
// --------------------------------------------------------------------------------------------------------------------

static_assert(fixed_t<int8_t, 5>{-128});
static_assert(fixed_t<int8_t, 5>{-1});
static_assert(!fixed_t<int8_t, 5>{0});
static_assert(fixed_t<int8_t, 5>{1});
static_assert(fixed_t<int8_t, 5>{127});

} // namespace conversions

// ====================================================================================================================
// Unary Arithmetic
// ====================================================================================================================

namespace unary_arithmetic {

static_assert(+fixed_t<int_t, 5>{10}.value == 10);
static_assert(-fixed_t<int_t, 5>{10}.value == -10);

} // namespace unary_arithmetic

// ====================================================================================================================
// Binary Arithmetic
// ====================================================================================================================

namespace binary_arithmetic {

static_assert(fixed_t<int_t, 5>{3} + fixed_t<int_t, 5>{7} == fixed_t<int_t, 5>{10});
static_assert(fixed_t<int_t, 5>{3} - fixed_t<int_t, 5>{7} == fixed_t<int_t, 5>{-4});

struct fixed_test_compound_assignment_t : Test
{
    using sut_t = fixed_t<int_t, 5>;
    sut_t expected{3};
    sut_t rhs{7};
};

TEST_F(fixed_test_compound_assignment_t, addition)
{
    ASSERT_EQ(&expected, &(expected += rhs));
}

TEST_F(fixed_test_compound_assignment_t, subtraction)
{
    ASSERT_EQ(&expected, &(expected -= rhs));
}

} // namespace binary_arithmetic

} // namespace
} // namespace crv
