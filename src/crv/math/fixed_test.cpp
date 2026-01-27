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
// Bool
// --------------------------------------------------------------------------------------------------------------------

static_assert(fixed_t<int8_t, 5>{-128}, "fixed_t: operator bool -128 failed");
static_assert(fixed_t<int8_t, 5>{-1}, "fixed_t: operator bool -1 failed");
static_assert(!fixed_t<int8_t, 5>{0}, "fixed_t: operator bool 0 failed");
static_assert(fixed_t<int8_t, 5>{1}, "fixed_t: operator bool 1 failed");
static_assert(fixed_t<int8_t, 5>{127}, "fixed_t: operator bool 127 failed");

} // namespace conversions

// ====================================================================================================================
// Unary Arithmetic
// ====================================================================================================================

namespace unary_arithmetic {

static_assert(+fixed_t<int_t, 5>{10}.value == 10, "fixed_t: unary plus failed");
static_assert(-fixed_t<int_t, 5>{10}.value == -10, "fixed_t: unary minus failed");

} // namespace unary_arithmetic

} // namespace
} // namespace crv
