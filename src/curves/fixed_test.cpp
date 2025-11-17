// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "fixed.hpp"
#include <curves/test.hpp>
#include <string>

namespace curves {
namespace {

const auto kMin = std::numeric_limits<int64_t>::min();
const auto kMax = std::numeric_limits<int64_t>::max();

// ----------------------------------------------------------------------------
// __curves_fixed_truncate_s64() Tests
// ----------------------------------------------------------------------------

struct CurvesFixedTruncateS64ShrTestParam {
  s64 value;
  unsigned int shift;
  s64 expected_result;

  friend auto operator<<(std::ostream& out,
                         const CurvesFixedTruncateS64ShrTestParam& src)
      -> std::ostream& {
    return out << "{" << src.value << ", " << src.shift << ", "
               << src.expected_result << "}";
  }
};

struct CurvesFixedTruncateS64ShrTest
    : TestWithParam<CurvesFixedTruncateS64ShrTestParam> {
  s64 value = GetParam().value;
  unsigned int shift = GetParam().shift;
  s64 expected_result = GetParam().expected_result;
};

TEST_P(CurvesFixedTruncateS64ShrTest, expected_result) {
  ASSERT_EQ(expected_result, __curves_fixed_truncate_s64_shr(value, shift));
}

// For each shift value (0, 1, 16, 32, 62, 63):
//   Test positive value +/- 1, + 0
//   Test zero +/- 1, + 0
//   Test negative exact multiple of (1 << shift) +/- 1, + 0
//   Test INT64_MIN + 1, + 0
CurvesFixedTruncateS64ShrTestParam truncate_s64_all_cases[] = {
    // shift: 0, special case; no truncation occurs

    // shift: 0, first positive boundary
    {(1LL << 0) + 1, 0, ((1LL << 0) + 1) >> 0},  // not biased
    {(1LL << 0) + 0, 0, ((1LL << 0) + 0) >> 0},  // not biased
    {(1LL << 0) - 1, 0, ((1LL << 0) - 1) >> 0},  // not biased

    // shift: 0, boundary at zero
    {0LL + 1, 0, ((0LL + 1) >> 0) + 0},  // not biased
    {0LL + 0, 0, ((0LL + 0) >> 0) + 0},  // not biased
    {0LL - 1, 0, ((0LL - 1) >> 0) + 0},  // not biased

    // shift: 0, first negative boundary
    {-(1LL << 0) + 1, 0, ((-(1LL << 0) + 1) >> 0) + 0},  // not biased
    {-(1LL << 0) + 0, 0, ((-(1LL << 0) + 0) >> 0) + 0},  // not biased
    {-(1LL << 0) - 1, 0, ((-(1LL << 0) - 1) >> 0) + 0},  // not biased

    // shift: 0, min
    {kMin + 1, 0, ((kMin + 1) >> 0) + 0},  // not biased
    {kMin + 0, 0, ((kMin + 0) >> 0) + 0},  // not biased

    // shift: 1

    // shift: 1, first positive boundary
    {(1LL << 1) + 1, 1, ((1LL << 1) + 1) >> 1},  // not biased
    {(1LL << 1) + 0, 1, ((1LL << 1) + 0) >> 1},  // not biased
    {(1LL << 1) - 1, 1, ((1LL << 1) - 1) >> 1},  // not biased

    // shift: 1, boundary at zero
    {0LL + 1, 1, ((0LL + 1) >> 1) + 0},  // not biased
    {0LL + 0, 1, ((0LL + 0) >> 1) + 0},  // not biased
    {0LL - 1, 1, ((0LL - 1) >> 1) + 1},  // rounds up

    // shift: 1, first negative boundary
    {-(1LL << 1) + 1, 1, ((-(1LL << 1) + 1) >> 1) + 1},  // rounds up
    {-(1LL << 1) + 0, 1, ((-(1LL << 1) + 0) >> 1) + 0},  // not biased
    {-(1LL << 1) - 1, 1, ((-(1LL << 1) - 1) >> 1) + 1},  // rounds up

    // shift: 1, min
    {kMin + 1, 1, ((kMin + 1) >> 1) + 1},  // rounds up
    {kMin + 0, 1, ((kMin + 0) >> 1) + 0},  // not biased

    // shift: 16

    // shift: 16, first positive boundary
    {(1LL << 16) + 1, 16, ((1LL << 16) + 1) >> 16},  // not biased
    {(1LL << 16) + 0, 16, ((1LL << 16) + 0) >> 16},  // not biased
    {(1LL << 16) - 1, 16, ((1LL << 16) - 1) >> 16},  // not biased

    // shift: 16, boundary at zero
    {0LL + 1, 16, ((0LL + 1) >> 16) + 0},  // not biased
    {0LL + 0, 16, ((0LL + 0) >> 16) + 0},  // not biased
    {0LL - 1, 16, ((0LL - 1) >> 16) + 1},  // rounds up

    // shift: 16, first negative boundary
    {-(1LL << 16) + 1, 16, ((-(1LL << 16) + 1) >> 16) + 1},  // rounds up
    {-(1LL << 16) + 0, 16, ((-(1LL << 16) + 0) >> 16) + 0},  // not biased
    {-(1LL << 16) - 1, 16, ((-(1LL << 16) - 1) >> 16) + 1},  // rounds up

    // shift: 16, min
    {kMin + 1, 16, ((kMin + 1) >> 16) + 1},  // rounds up
    {kMin + 0, 16, ((kMin + 0) >> 16) + 0},  // not biased

    // shift: 32

    // shift: 32, first positive boundary
    {(1LL << 32) + 1, 32, ((1LL << 32) + 1) >> 32},  // not biased
    {(1LL << 32) + 0, 32, ((1LL << 32) + 0) >> 32},  // not biased
    {(1LL << 32) - 1, 32, ((1LL << 32) - 1) >> 32},  // not biased

    // shift: 32, boundary at zero
    {0LL + 1, 32, ((0LL + 1) >> 32) + 0},  // not biased
    {0LL + 0, 32, ((0LL + 0) >> 32) + 0},  // not biased
    {0LL - 1, 32, ((0LL - 1) >> 32) + 1},  // rounds up

    // shift: 32, first negative boundary
    {-(1LL << 32) + 1, 32, ((-(1LL << 32) + 1) >> 32) + 1},  // rounds up
    {-(1LL << 32) + 0, 32, ((-(1LL << 32) + 0) >> 32) + 0},  // not biased
    {-(1LL << 32) - 1, 32, ((-(1LL << 32) - 1) >> 32) + 1},  // rounds up

    // shift: 32, min
    {kMin + 1, 32, ((kMin + 1) >> 32) + 1},  // rounds up
    {kMin + 0, 32, ((kMin + 0) >> 32) + 0},  // not biased

    // shift: 62, last shift with no special cases

    // shift: 62, first positive boundary
    {(1LL << 62) + 1, 62, ((1LL << 62) + 1) >> 62},  // not biased
    {(1LL << 62) + 0, 62, ((1LL << 62) + 0) >> 62},  // not biased
    {(1LL << 62) - 1, 62, ((1LL << 62) - 1) >> 62},  // not biased

    // shift: 62, boundary at zero
    {0LL + 1, 62, ((0LL + 1) >> 62) + 0},  // not biased
    {0LL + 0, 62, ((0LL + 0) >> 62) + 0},  // not biased
    {0LL - 1, 62, ((0LL - 1) >> 62) + 1},  // rounds up

    // shift: 62, first negative boundary
    {-(1LL << 62) + 1, 62, ((-(1LL << 62) + 1) >> 62) + 1},  // rounds up
    {-(1LL << 62) + 0, 62, ((-(1LL << 62) + 0) >> 62) + 0},  // not biased
    {-(1LL << 62) - 1, 62, ((-(1LL << 62) - 1) >> 62) + 1},  // rounds up

    // shift: 62, min
    {kMin + 1, 62, ((kMin + 1) >> 62) + 1},  // rounds up
    {kMin + 0, 62, ((kMin + 0) >> 62) + 0},  // not biased

    // shift: 63: special case; no positive integers, only one negative boundary

    // shift: 63, boundary at zero
    {0LL + 1, 63, ((0LL + 1) >> 63) + 0},  // not biased
    {0LL + 0, 63, ((0LL + 0) >> 63) + 0},  // not biased
    {0LL - 1, 63, ((0LL - 1) >> 63) + 1},  // rounds up

    // shift: 63, min
    {kMin + 1, 63, ((kMin + 1) >> 63) + 1},  // rounds up
    {kMin + 0, 63, ((kMin + 0) >> 63) + 0},  // not biased
};

INSTANTIATE_TEST_SUITE_P(all_cases, CurvesFixedTruncateS64ShrTest,
                         ValuesIn(truncate_s64_all_cases));

// ----------------------------------------------------------------------------
// __curves_fixed_rescale_error_s64
// ----------------------------------------------------------------------------

struct CurvesFixedRescaleErrorS64TestParam {
  s64 value;
  int shift;
  s64 expected_result;
};

struct CurvesFixedRescaleErrorS64Test
    : TestWithParam<CurvesFixedRescaleErrorS64TestParam> {
  s64 value = GetParam().value;
  int shift = GetParam().shift;
  s64 expected_result = GetParam().expected_result;
};

TEST_P(CurvesFixedRescaleErrorS64Test, expected_result) {
  ASSERT_EQ(expected_result, __curves_fixed_rescale_error_s64(value, shift));
}

const CurvesFixedRescaleErrorS64TestParam rescale_error_s64_all_cases[] = {
    {0, 0, 0},    // value == 0 && shift == 0
    {0, -1, 0},   // value == 0 && shift < 0
    {-1, -1, 0},  // value < 0 && shift < 0
    {1, -1, 0},   // value > 0 && shift < 0

    {1, 0, S64_MAX},   // value > 0 && shift == 0
    {-1, 0, S64_MIN},  // value < 0 && shift == 0

    {1, 1, S64_MAX},   // value > 0 && shift > 0
    {-1, 1, S64_MIN},  // value < 0 && shift > 0

    {S64_MAX, 1, S64_MAX},  // value > 0 && shift > 0; edge case
    {S64_MIN, 1, S64_MIN},  // value < 0 && shift > 0; edge case
};

INSTANTIATE_TEST_SUITE_P(all_cases, CurvesFixedRescaleErrorS64Test,
                         ValuesIn(rescale_error_s64_all_cases));

// ----------------------------------------------------------------------------
// curves_fixed_rescale_s64
// ----------------------------------------------------------------------------

struct CurvesFixedRescaleS64TestParam {
  s64 value;
  unsigned int frac_bits;
  unsigned int output_frac_bits;
  s64 expected_result;
};

struct CurvesFixedRescaleS64Test
    : TestWithParam<CurvesFixedRescaleS64TestParam> {
  s64 value = GetParam().value;
  unsigned int frac_bits = GetParam().frac_bits;
  unsigned int output_frac_bits = GetParam().output_frac_bits;
  s64 expected_result = GetParam().expected_result;
};

TEST_P(CurvesFixedRescaleS64Test, expected_result) {
  ASSERT_EQ(expected_result,
            curves_fixed_rescale_s64(value, frac_bits, output_frac_bits));
}

/*
  Invalid scale tests for truncation with frac_bits/output_frac_bits >= 64.

  Test coverage was systematically derived by enumerating all paths through
  the conditional logic.

  Conditions:
    A = frac_bits >= 64
    B = output_frac_bits >= 64
    C = value == 0
    D = output_frac_bits < frac_bits

  Return 0 paths: (A||B) && (C||D) → AC, AD, BC, ABCD tested
  Saturate paths: (A||B) && !C && !D → A!C!D, B!C!D tested (both signs)
*/
const CurvesFixedRescaleS64TestParam rescale_s64_invalid_scales[] = {
    // Path AC: frac_bits boundary, zero value
    {0, 64, 32, 0},

    // Path AD: frac_bits boundary, right shift
    {100, 64, 63, 0},

    // Path BC: output_frac_bits boundary, zero value
    {0, 32, 64, 0},

    // Path ABCD: both boundaries, multiple conditions active
    {0, 64, 64, 0},

    // Path A!C!D: frac_bits boundary, left shift, positive saturation
    {1, 64, 65, kMax},

    // Path A!C!D: frac_bits boundary, left shift, negative saturation
    {-1, 64, 65, kMin},

    // Path B!C!D: output_frac_bits boundary, positive saturation
    {kMax, 32, 64, kMax},

    // Path B!C!D: output_frac_bits boundary, negative saturation
    {kMin, 32, 64, kMin},
};

INSTANTIATE_TEST_SUITE_P(invalid_scales, CurvesFixedRescaleS64Test,
                         ValuesIn(rescale_s64_invalid_scales));

/*
  Systematic boundary value analysis coverage:

  Value categories: zero, positive (positive), negative (negative),
                    large_positive, large_negative
  Shift relationship: right (output < frac), equal, left (output > frac)
  Parameter boundaries: min (0), max (63), mid (mid-range)

  Coverage matrix (showing key combinations tested):
  right + negative + mid        - negative values through helper
  right + zero + mid            - zero special case
  right + positive + MIN_OUT    - boundary: output_frac_bits at min
  right + positive + mid        - basic right shift path
  right + positive + max_FRAC   - boundary: frac_bits at max valid
  right + positive + large_DIFF - extreme shift amount

  equal + zero + mid            - zero with no shift
  equal + positive + MIN        - boundary: both params at min
  equal + positive + mid        - basic equal path
  equal + positive + max        - boundary: both params at max

  left + positive + mid         - basic left shift path
  left + negative + mid         - negative left shift
  left + zero + mid             - zero with left shift
  left + positive + max_OUT     - boundary: output_frac_bits at max
  left + positive + large_DIFF  - extreme left shift amount
*/
const CurvesFixedRescaleS64TestParam rescale_s64_valid_scales[] = {
    // output_frac_bits < frac_bits
    {35 << 16, 48, 32, 35},

    // output_frac_bits 0 frac_bits
    {35 << 16, 40, 40, 35 << 16},

    // output_frac_bits > frac_bits
    {35, 32, 48, 35 << 16},
};

INSTANTIATE_TEST_SUITE_P(valid_scales, CurvesFixedRescaleS64Test,
                         ValuesIn(rescale_s64_valid_scales));

// ----------------------------------------------------------------------------
// Integer Conversions Tests
// ----------------------------------------------------------------------------

// Symmetric
// ----------------------------------------------------------------------------

/*
  These are tests that don't truncate the fixed value, so they are the same
  in either direction.
*/

struct SymmetricIntegersParam {
  int64_t integer_value;
  unsigned int frac_bits;
  s64 fixed_value;

  friend auto operator<<(std::ostream& out, const SymmetricIntegersParam& src)
      -> std::ostream& {
    return out << "{" << src.integer_value << ", " << src.frac_bits << ", "
               << src.fixed_value << "}";
  }
};

struct FixedConversionsTestSymmetricIntegers
    : public TestWithParam<SymmetricIntegersParam> {};

TEST_P(FixedConversionsTestSymmetricIntegers, to_fixed) {
  const auto param = GetParam();

  const auto expected = param.fixed_value;
  const auto actual =
      curves_fixed_from_integer(param.integer_value, param.frac_bits);

  ASSERT_EQ(expected, actual);
}

TEST_P(FixedConversionsTestSymmetricIntegers, to_integer) {
  const auto param = GetParam();

  const auto expected = param.integer_value;
  const auto actual =
      curves_fixed_to_integer(param.fixed_value, param.frac_bits);

  ASSERT_EQ(expected, actual);
}

const SymmetricIntegersParam symmetric_integer_params[] = {
    // end of negative q63.0 range
    {kMin, 0, kMin},

    // end of q62.1 range
    {-1ll << 62, 1, (-1ll << 62) << 1},

    // end of q47.16 range
    {-1ll << 47, 1, (-1ll << 47) << 1},
    {-1ll << 47, 8, (-1ll << 47) << 8},
    {-1ll << 47, 16, (-1ll << 47) << 16},

    // end of q31.32 range
    {-1ll << 31, 1, (-1ll << 31) << 1},
    {-1ll << 31, 16, (-1ll << 31) << 16},
    {-1ll << 31, 32, (-1ll << 31) << 32},

    // end of q15.48 range
    {-1ll << 15, 1, (-1ll << 15) << 1},
    {-1ll << 15, 24, (-1ll << 15) << 24},
    {-1ll << 15, 48, (-1ll << 15) << 48},

    // -2
    {-2, 1, -2ll << 1},
    {-2, 32, -2ll << 32},
    {-2, 61, -2ll << 61},

    // -1
    {-1, 1, -1ll << 1},
    {-1, 32, -1ll << 32},
    {-1, 62, -1ll << 62},

    // zero
    {0, 1, 0},
    {0, 32, 0},
    {0, 63, 0},

    // 1
    {1, 1, 1ll << 1},
    {1, 32, 1ll << 32},
    {1, 62, 1ll << 62},

    // 2
    {2, 1, 2ll << 1},
    {2, 32, 2ll << 32},
    {2, 61, 2ll << 61},

    // end of q15.48 range
    {(1ll << 15) - 1, 1, ((1ll << 15) - 1) << 1},
    {(1ll << 15) - 1, 24, ((1ll << 15) - 1) << 24},
    {(1ll << 15) - 1, 48, ((1ll << 15) - 1) << 48},

    // end of q31.32 range
    {(1ll << 31) - 1, 1, ((1ll << 31) - 1) << 1},
    {(1ll << 31) - 1, 16, ((1ll << 31) - 1) << 16},
    {(1ll << 31) - 1, 32, ((1ll << 31) - 1) << 32},

    // end of q47.16 range
    {(1ll << 47) - 1, 1, ((1ll << 47) - 1) << 1},
    {(1ll << 47) - 1, 8, ((1ll << 47) - 1) << 8},
    {(1ll << 47) - 1, 16, ((1ll << 47) - 1) << 16},

    // end of q62.1 range
    {(1ll << 62) - 1, 1, ((1ll << 62) - 1) << 1},

    // end of q63.0 range
    {kMax, 0, kMax},
};

INSTANTIATE_TEST_SUITE_P(all_conversions, FixedConversionsTestSymmetricIntegers,
                         ValuesIn(symmetric_integer_params));

// Truncation
// ----------------------------------------------------------------------------

/*
  These test that fixed->integer conversions always truncate, rather than the
  default fixed-point behavior to round towards negative infinity that it gets
  from using integers.
*/

struct IntegerTruncationParam {
  s64 fixed_value;
  unsigned int frac_bits;
  int64_t integer_value;

  friend auto operator<<(std::ostream& out, const IntegerTruncationParam& src)
      -> std::ostream& {
    return out << "{" << src.integer_value << ", " << src.frac_bits << ", "
               << src.fixed_value << "}";
  }
};

struct FixedConversionsTestIntegerTruncation
    : public TestWithParam<IntegerTruncationParam> {};

TEST_P(FixedConversionsTestIntegerTruncation, conversion_to_integer_truncates) {
  const auto param = GetParam();

  const auto expected = param.integer_value;
  const auto actual =
      curves_fixed_to_integer(param.fixed_value, param.frac_bits);

  ASSERT_EQ(expected, actual);
}

const IntegerTruncationParam integer_truncation_test_params[] = {
    {-4611686018427387904LL, 61, -2},  // = -2, floors to -2, truncates to -2
    {-4611686018427387903LL, 61, -1},  // < -2, floors to -2, truncates to -1
    {-3458764513820540928LL, 61, -1},  // = -1.5, floors to -2, truncates to -1
    {-3458764513820540927LL, 61, -1},  // < -1.5, floors to -2, truncates to -1
    {-2305843009213693952LL, 61, -1},  // = -1, floors to -1, truncates to -1
    {-2305843009213693951LL, 61, 0},   // < -1, floors to -1, truncates to 0
    {-1152921504606846976LL, 61, 0},   // = -0.5, floors to -1, truncates to 0
    {-1152921504606846975LL, 61, 0},   // < -0.5, floors -1, truncates to 0

    {1LL, 61, 0},   // > 0, floors to 0, truncates to 0
    {0LL, 61, 0},   // = 0, floors to 0, truncates to 0
    {-1LL, 61, 0},  // < 0, floors to 0, truncates to 0

    {1152921504606846975LL, 61, 0},  // < 0.5, floors to 0, truncates to 0
    {1152921504606846976LL, 61, 0},  // = 0.5, floors to 0, truncates to 0
    {2305843009213693951LL, 61, 0},  // < 1, floors to 0, truncates to 0
    {2305843009213693952LL, 61, 1},  // = 1, floors to 1, truncates to 1
    {3458764513820540927LL, 61, 1},  // < 1.5, floors to 1, truncates to 1
    {3458764513820540928LL, 61, 1},  // = 1.5, floors to 1, truncates to 1
    {4611686018427387903LL, 61, 1},  // < 2, floors to 1, truncates to 1
    {4611686018427387904LL, 61, 2},  // = 2, floors to 2, truncates to 2
};

INSTANTIATE_TEST_SUITE_P(high_precision, FixedConversionsTestIntegerTruncation,
                         ValuesIn(integer_truncation_test_params));

// These test edge cases right at their edge, one inside, and the adjacent one
// outside.
const IntegerTruncationParam integer_truncation_boundary_test_params[] = {
    // frac_bits = 0: Special case, no rounding.
    {kMin, 0, kMin},
    {kMin + 1, 0, kMin + 1},
    {kMax - 1, 0, kMax - 1},
    {kMax, 0, kMax},

    // frac_bits = 1: Lowest precision that isn't just integers.
    {kMin, 1, kMin >> 1},
    {kMin + 1, 1, (kMin >> 1) + 1},
    {kMax - 2, 1, (kMax >> 1) - 1},
    {kMax - 1, 1, kMax >> 1},
    {kMax, 1, kMax >> 1},

    // frac_bits = 32: Typical precision
    {kMin, 32, kMin >> 32},
    {kMin + 1, 32, (kMin >> 32) + 1},
    {kMax - (1LL << 32), 32, (kMax >> 32) - 1},
    {kMax - (1LL << 32) + 1, 32, (kMax >> 32)},
    {kMax, 32, (kMax >> 32)},

    // frac_bits = 61: Highest precision that doesn't hit range boundary.
    {kMin, 61, -4},
    {kMin + 1, 61, -3},
    {kMax - (1LL << 61), 61, 2},
    {kMax - (1LL << 61) + 1, 61, 3},
    {kMax, 61, 3},

    // frac_bits = 62: Maximum precision
    {kMin, 62, -2},
    {kMin + 1, 62, -1},
    {kMax - (1LL << 62), 62, 0},
    {kMax - (1LL << 62) + 1, 62, 1},
    {kMax, 62, 1},
};

INSTANTIATE_TEST_SUITE_P(boundaries, FixedConversionsTestIntegerTruncation,
                         ValuesIn(integer_truncation_boundary_test_params));

// ----------------------------------------------------------------------------
// Double Conversions Tests
// ----------------------------------------------------------------------------

// Double -> Fixed
// ----------------------------------------------------------------------------

struct DoubleConversionTestParam {
  s64 fixed_value;
  unsigned int frac_bits;
  double double_value;
};

struct FixedConversionTestFixedFromDouble
    : TestWithParam<DoubleConversionTestParam> {};

TEST_P(FixedConversionTestFixedFromDouble, from_double) {
  const auto param = GetParam();
  const auto actual =
      curves_fixed_from_double(param.double_value, param.frac_bits);
  ASSERT_EQ(param.fixed_value, actual);
}

// clang-format off
const DoubleConversionTestParam from_double_params[] = {
  /*
    The truncation from double to fixed is different than the truncation from
    fixed to integer. The conversion relies on the double->integer cast, which
    performs real truncation, rounding towards zero.

    These tests show this for frac_bits = 0, which is really just round
    tripping the truncation with no scaling.
  */
  {-123, 0, -123.45},
  {123,  0,  123.45},
  {0,    0,  -0.9},
  {0,    0,  0.9},

  /*
    Normal values for frac_bits = 32:
      2ll << 32 -> 2
      1ll << 31 -> 0.5
      1ll << 30 -> 0.25
  */
  {(-2ll << 32) - ((1ll << 31) | (1ll << 30)), 32, -2.75},
  {(2ll << 32) + ((1ll << 31) | (1ll << 30)),  32, 2.75},

  /*
    The smallest bit at precision 32 is 1/2^32. 2^-33 is half of that, so the
    fixed point value we're generating here is actually 2^-33*(1 << 32) = 0.5,
    which truncates to 0.

    These tests show it truncates to zero from both sides.
  */
  {0, 32, -std::ldexp(1.0, -33)},
  {0, 32, std::ldexp(1.0, -33)},

  /*
    Min and max representable values for frac_bits = 0.

    Ideally, we'd test against max, but it is a 63-bit number. A double only
    has 53 bits of precision, so it can't be stored precisely in a double. If
    we were to try to round trip it, the runtime would have to pick the closest
    representable double, which in this case, causes it to round up to 2^64.
    The value in the double is then larger than max. Converting an out of range
    double to an integer is undefined behavior. In this specific case, on x64,
    converting back just happens to give the value min. That is about as
    different from the value we started with as could be, so the test fails.

    Instead, we use the largest round-trippable integer, which is:
      max - 1023 = (2^63 - 1) - (2^10 - 1) = 2^63 - 2^10

    min is representable, so we use it directly.
  */
  {kMin,        0, static_cast<double>(kMin)},
  {kMax - 1023, 0, static_cast<double>(kMax - 1023)},

  // Min and max representable values for frac_bits = 32
  {kMin,                    32, -static_cast<double>(1ll << 31)},
  {((1ll << 31) - 1) << 32, 32, static_cast<double>((1ll << 31) - 1)},

  // Min and max representable values for frac_bits = 62
  {kMin,      62, -2.0},
  {1ll << 62, 62, 1.0},
};
// clang-format on

INSTANTIATE_TEST_SUITE_P(all, FixedConversionTestFixedFromDouble,
                         ValuesIn(from_double_params));

// Fixed -> Double
// ----------------------------------------------------------------------------

struct FixedConversionTestFixedToDouble
    : TestWithParam<DoubleConversionTestParam> {};

TEST_P(FixedConversionTestFixedToDouble, to_double) {
  const auto param = GetParam();
  const auto actual =
      curves_fixed_to_double(param.fixed_value, param.frac_bits);
  ASSERT_DOUBLE_EQ(param.double_value, actual);
}

// clang-format off
const DoubleConversionTestParam to_double_params[] = {
  // frac_bits = 0 is just the original integers as doubles with no scaling.
  {123, 0, 123.0},
  {-456,0,  -456.0},

  // frac_bits = 32, normal values with full precision
  {(2ll << 32) | (1ll << 31),  32, 2.5},
  {(-3ll << 32) | (1ll << 31), 32, -2.5},
  {1,                          32, std::ldexp(1.0, -32)}, // 1/2^32
  {-1,                         32, -std::ldexp(1.0, -32)},

  /*
    frac_bits = 60 causes precision loss when converting to 53-bit double.

    In q3.60:
      (1ll << 60) is 1.0
      (1ll << 0) is 2^-60 (60 - 0 = 60)
      (1ll << 6) is 2^-54 (60 - 6 = 54)
      (1ll << 7) is 2^-53 (60 - 7 = 53)

    1 + 2^-60 will lose the 2^-60 part, (1ll << 0) bit is cleared
    1 + 2^-54 will lose the 2^-54 part, (1ll << 6) bit is cleared
    1 + 2^-53 will keep the 2^-53 part, (1ll << 7) bit is set
  */
  {(1ll << 60) | (1ll << 0), 60, 1.0}, // The 2^-60 part is lost
  {(1ll << 60) | (1ll << 6), 60, 1.0}, // The 2^-54 part is lost
  {(1ll << 60) | (1ll << 7), 60, 1.0 + std::ldexp(1.0, -53)}, // bit is kept
};
// clang-format on

INSTANTIATE_TEST_SUITE_P(all, FixedConversionTestFixedToDouble,
                         ValuesIn(to_double_params));

// ----------------------------------------------------------------------------
// Fixed Test
// ----------------------------------------------------------------------------

struct FixedTest : Test {};

TEST_F(FixedTest, one_highest_precision) {
  const auto frac_bits = 62;
  const auto expected = 4611686018427387904ll;

  const auto actual = curves_fixed_const_1(frac_bits);

  ASSERT_EQ(expected, actual);
}

TEST_F(FixedTest, one_lowest_precision) {
  const auto frac_bits = 0;
  const auto expected = 1ll;

  const auto actual = curves_fixed_const_1(frac_bits);

  ASSERT_EQ(expected, actual);
}

// ----------------------------------------------------------------------------
// Constants Test
// ----------------------------------------------------------------------------

struct ConstantsTestParam {
  std::string name;
  s64 (*constant_func)(unsigned int);
  double expected_value;
  unsigned int frac_bits;
  double tolerance;

  friend auto operator<<(std::ostream& out, const ConstantsTestParam& src)
      -> std::ostream& {
    return out << "{" << src.expected_value << ", " << src.frac_bits << ", "
               << src.tolerance << "}";
  }
};

struct FixedConstantsTest : public TestWithParam<ConstantsTestParam> {};

TEST_P(FixedConstantsTest, verify_constants) {
  const auto param = GetParam();

  const auto actual_fixed = param.constant_func(param.frac_bits);
  const auto one_fixed = curves_fixed_const_1(param.frac_bits);

  const auto actual_double = double(actual_fixed) / double(one_fixed);

  if (param.tolerance == 0.0) {
    ASSERT_DOUBLE_EQ(param.expected_value, actual_double);
  } else {
    ASSERT_NEAR(param.expected_value, actual_double, param.tolerance);
  }
}

const ConstantsTestParam constants_test_params[] = {
    // 1
    {"1_high", curves_fixed_const_1, 1, CURVES_FIXED_1_FRAC_BITS, 0.0},
    {"1_medium", curves_fixed_const_1, 1, CURVES_FIXED_1_FRAC_BITS / 2, 0.0},
    {"1_low", curves_fixed_const_1, 1, 1, 0.0},

    // e
    {"e_high", curves_fixed_const_e, M_E, CURVES_FIXED_E_FRAC_BITS, 0.0},
    {"e_medium", curves_fixed_const_e, M_E, CURVES_FIXED_E_FRAC_BITS / 2,
     6.0e-10},
    {"e_low", curves_fixed_const_e, M_E, 1, 2.2e-1},

    // ln(2)
    {"ln2_high", curves_fixed_const_ln2, std::log(2.0),
     CURVES_FIXED_LN2_FRAC_BITS, 0.0},
    {"ln2_medium", curves_fixed_const_ln2, std::log(2.0),
     CURVES_FIXED_LN2_FRAC_BITS / 2, 4.3e-10},
    {"ln2_low", curves_fixed_const_ln2, std::log(2.0), 1, 2.0e-1},

    // pi
    {"pi_high", curves_fixed_const_pi, M_PI, CURVES_FIXED_PI_FRAC_BITS, 0.0},
    {"pi_medium", curves_fixed_const_pi, M_PI, CURVES_FIXED_PI_FRAC_BITS / 2,
     1.3e-10},
    {"pi_low", curves_fixed_const_pi, M_PI, 1, 1.5e-1},
};

INSTANTIATE_TEST_SUITE_P(all_constants, FixedConstantsTest,
                         ValuesIn(constants_test_params),
                         [](const TestParamInfo<ConstantsTestParam>& info) {
                           return info.param.name;
                         });

#if 0
// ----------------------------------------------------------------------------
// Multiplication Tests
// ----------------------------------------------------------------------------

// Test Parameter
// ----------------------------------------------------------------------------

struct MultiplicationParam {
  s64 multiplicand;
  s64 multiplier;
  int desired_shift;
  s64 expected_result;
  s64 expected_bias;

  friend auto operator<<(std::ostream& out, const MultiplicationParam& src)
      -> std::ostream& {
    return out << "{" << src.multiplicand << ", " << src.multiplier << ", "
               << src.desired_shift << ", " << src.expected_result << "}";
  }
};

// Test Fixture
// ----------------------------------------------------------------------------

/*
  Parameterized test fixture for fixed-point multiplication.

  curves_fixed_multiply() takes 5 parameters, but they are not independent. The
  only real, independent parameters are the multiplicand, multiplier, and the
  internal shift it calculates from the 3 precisions. The factors and
  precisions are commutative, so the whole testable space reduces from
  5-dimensional to 2.

  The internal shift is calculated as:

    `shift = output_frac_bits - (multiplicand_frac_bits + multiplier_frac_bits)`

  The tests are parameterized by `multiplicand`, `multiplier`, and
  `desired_shift`. The test harness then calculates the internal `shift`
  indirectly as `shift = desired_shift` using the 3 frac_bits parameters.

  In the test cases, this means:
  - A positive `desired_shift` produces a left shift.
  - A negative `desired_shift` produces a right shift.
  - A zero `desired_shift` produces no shift; only truncation occurs.

  The test cases (e.g., `via_multiplicand`, `via_output_frac_bits`) drive this
  internal `shift` in different ways, but all should produce the same result
  for the same `desired_shift`. By analogy, they're like different doors going
  into the same room.

  Since this is white-box testing, we know the implementation relies on the
  commutativity of * (for factors) and + (for precisions). We deliberately omit
  tests that just swap arguments left for right. This reduces the number of
  test cases and the number of unique test parameterizations with no loss of
  generality.
*/
struct FixedMultiplicationTest : testing::TestWithParam<MultiplicationParam> {
  const s64 multiplicand = GetParam().multiplicand;
  const s64 multiplier = GetParam().multiplier;
  const int desired_shift = GetParam().desired_shift;
  auto expected_result(unsigned int multiplicand_frac_bits,
                       unsigned int multiplier_frac_bits) const noexcept
      -> s64 {
    const auto unbiased_result = GetParam().expected_result;
    const auto value = static_cast<int128_t>(multiplicand) * multiplier;
    if (value >= 0) return unbiased_result;

    int128_t sign_mask = value >> 127;

    const auto frac_bits = multiplicand_frac_bits + multiplier_frac_bits;
    int128_t bias = ((1LL << frac_bits) - 1) & sign_mask;
    return value + bias;
  }
};

// Test Cases
// ----------------------------------------------------------------------------

/*
  Splits shift between multiplicand (for right shifts) and output (for left
  shifts).

  In this test case, all of the desired shift comes from either the
  multiplicand or output. Which is chosen depends on the shift's sign so not to
  drive unsigned values negative.
*/
TEST_P(FixedMultiplicationTest, via_multiplicand_and_output) {
  const unsigned int multiplicand_frac_bits =
      static_cast<unsigned int>((desired_shift < 0) ? -desired_shift : 0);
  const unsigned int multiplier_frac_bits = 0;
  const unsigned int output_frac_bits =
      static_cast<unsigned int>((desired_shift > 0) ? desired_shift : 0);

  const auto actual_result =
      curves_fixed_multiply(multiplicand_frac_bits, multiplicand,
                            multiplier_frac_bits, multiplier, output_frac_bits);

  ASSERT_EQ(expected_result(multiplicand_frac_bits, multiplier_frac_bits),
            actual_result);
}

// Distributes right shifts between both inputs. Left shifts come from output.
TEST_P(FixedMultiplicationTest, via_all_inputs_and_output) {
  const unsigned int total_input_bits =
      static_cast<unsigned int>((desired_shift < 0) ? -desired_shift : 0);
  const unsigned int multiplicand_frac_bits = total_input_bits / 2;
  const unsigned int multiplier_frac_bits =
      total_input_bits - multiplicand_frac_bits;
  const unsigned int output_frac_bits =
      static_cast<unsigned int>((desired_shift > 0) ? desired_shift : 0);

  const auto actual_result =
      curves_fixed_multiply(multiplicand_frac_bits, multiplicand,
                            multiplier_frac_bits, multiplier, output_frac_bits);

  ASSERT_EQ(expected_result(multiplicand_frac_bits, multiplier_frac_bits),
            actual_result);
}

/*
  Biases shift by a large base precision to input and output, then reduces
  output to acheive desired shift, spilling over to input if it underflows.
  This tests a mix of both parameters for both positive and negative shifts.
*/
TEST_P(FixedMultiplicationTest, via_base_precision_mixed) {
  const int kBasePrecision = 64;
  const unsigned int multiplicand_frac_bits = kBasePrecision;
  unsigned int multiplier_frac_bits = 0;
  unsigned int output_frac_bits = 0;
  if (kBasePrecision + desired_shift >= 0) {
    // desired_shift does not reduce biased output_frac_bits past zero.
    output_frac_bits =
        static_cast<unsigned int>(kBasePrecision + desired_shift);
  } else {
    // output_frac_bits would underflow, so increase multiplier instead.
    multiplier_frac_bits =
        static_cast<unsigned int>(-(kBasePrecision + desired_shift));
  }

  const auto actual_result =
      curves_fixed_multiply(multiplicand_frac_bits, multiplicand,
                            multiplier_frac_bits, multiplier, output_frac_bits);

  ASSERT_EQ(expected_result(multiplicand_frac_bits, multiplier_frac_bits),
            actual_result);
}

#if 1
// Test Data
// ----------------------------------------------------------------------------

/*
  Zero Operands

  Any multiplication involving zero should result in zero, regardless of shift.
*/
// clang-format off
const MultiplicationParam multiplication_zero_cases[] = {
    {0, 1, 0, 0},
    {0, -1, 0, 0},
    {-1, 0, 0, 0},
    {0, -100, -5, 0},
    {-100, 0, -5, 0},
    {0, -(1LL << 62), -32, 0},
    {0, 0, 1, 0},
};
// clang-format on

INSTANTIATE_TEST_SUITE_P(zero_cases, FixedMultiplicationTest,
                         testing::ValuesIn(multiplication_zero_cases));

/*
  No Shift (desired_shift = 0)

  Tests truncation and overflow when converting 128-bit product to 64-bit.
*/
// clang-format off
const MultiplicationParam multiplication_no_shift_cases[] = {
    {(1LL << 32), (1LL << 32), 0, kMax},
    {kMax, kMax, 0, kMax},
    {kMax, 2, 0, kMax},

    // Simple signs
    {1, 1, 0, 1},
    {-1, 1, 0, -1},
    {-1, -1, 0, 1},
    {-1, 100, 0, -100},

    // Large values
    {1LL << 62, 1, 0, 1LL << 62},
    {-(1LL << 62), 1, 0, -(1LL << 62)},
    {-(1LL << 62), -1, 0, 1LL << 62},
    {-(1LL << 61), 2, 0, -(1LL << 62)},
    {-(1LL << 61), -2, 0, 1LL << 62},

    // Boundary: max/min
    {kMax, 1, 0, kMax},
    {kMax, -1, 0, -kMax},
    {-kMax, -1, 0, kMax},

    // Overflow on truncation (product > 64 bits)
};
// clang-format on

INSTANTIATE_TEST_SUITE_P(no_shift_cases, FixedMultiplicationTest,
                         testing::ValuesIn(multiplication_no_shift_cases));

/*
  Negative (Right) Shifts (-128 < desired_shift < 0)

  Tests normal operation, 128-bit intermediate products, and rounding.
*/
// clang-format off
const MultiplicationParam multiplication_right_shift_cases[] = {
    // Simple positive
    {1, 1, -1, 0}, 
    {15, 26, -2, (15 * 26 >> 2)},
    {89, 11, -3, 89 * 11 >> 3},

    // Large positive values with shifts
    {1LL << 62, 1, -1, 1LL << 61},
    {1LL << 62, 1, -61, 2},
    {1LL << 62, 1, -62, 1},
    {1LL << 62, 1, -63, 0},
    {1LL << 61, 2, -62, 1},
    {1LL << 60, 4, -62, 1},

    // Values requiring > 64 bits internally
    {1LL << 32, 1LL << 32, -32, 1LL << 32}, // (1 << 64) >> 32
    {1LL << 40, 1LL << 40, -48, 1LL << 32}, // (1 << 80) >> 48
    {1LL << 50, 1LL << 50, -68, 1LL << 32}, // (1 << 100) >> 68
    {1000000000LL, 1000000000LL, -20, 953674316406LL},
    {100LL << 32, 200LL << 32, -63, 100LL * 200LL << 1}, // (X << 64) >> 63

    // Fixed point values
    {1447LL << 32, 13LL << 32, -32, 1447LL * 13LL << 32},

    // All sign combinations
    {-15, 26, -2, (-15 * 26 >> 2) + 0*1},
    {15, -26, -2, (15 * -26 >> 2) + 0*1},
    {-15, -26, -2, 15 * 26 >> 2},
    {-1447LL << 32, 13LL << 32, -32, -1447LL * 13LL << 32},
    {1447LL << 32, -13LL << 32, -32, -1447LL * 13LL << 32},
    {-1447LL << 32, -13LL << 32, -32, 1447LL * 13LL << 32},

    // Large negative values
    {-(1LL << 62), 1, -62, -1},
    {-(1LL << 62), -1, -62, 1},

    // Boundary: max/min
    {kMax, 2, -1, kMax},   // ((1 << 64) - 2) >> 1 = (1 << 63) - 1 = kMax
    {-kMax, 2, -1, -kMax},

    // Boundary: 127-bit shift
    {1LL << 63, 1LL << 63, -126, 1}, // (1 << 126) >> 126 = 1
    {1LL << 63, 1LL << 63, -127, 0}, // (1 << 126) >> 127 = 0.5

    // (kMin*-1) >> 1 = (1 << 63) >> 1 = 1 << 62
    {kMin, -1, -1, 1LL << 62},
};
// clang-format on

INSTANTIATE_TEST_SUITE_P(right_shift_cases, FixedMultiplicationTest,
                         testing::ValuesIn(multiplication_right_shift_cases));

/*
  Positive (Left) Shifts (0 < desired_shift < 64)

  Tests normal operation and overflow into sign bit or out of 64 bits.
*/
// clang-format off
const MultiplicationParam multiplication_left_shift_cases[] = {
    {1, 1, 1, 1LL << 1},
    {1, 1, 32, 1LL << 32},
    {1LL << 10, 1LL << 10, 20, 1LL << 40}, // (2^20) << 20 = 2^40
    {100, 200, 10, (100LL * 200) << 10},
    {-100, 200, 10, (-100LL * 200) << 10},

    // Boundary: shift to MSB
    {1, 1, 62, 1LL << 62},
    {2, 1, 62, kMax},
    {1, 1, 63, kMax},
    {1, -1, 63, kMin},

    // Boundary: overflow
    {2, 2, 63, kMax},
};
// clang-format on

INSTANTIATE_TEST_SUITE_P(left_shift_cases, FixedMultiplicationTest,
                         testing::ValuesIn(multiplication_left_shift_cases));

/*
  Extreme Negative (Right) Shift (desired_shift <= -128)

  Tests the `shift <= -128` branch, which should always return 0.
*/
// clang-format off
const MultiplicationParam multiplication_extreme_right_shift_cases[] = {
    // Boundary
    {1, 1, -128, 0},
    {100, 200, -128, 0},
    {kMax, kMax, -128, 0},

    // Signs
    {-1, 1, -128, 0},
    {-1, -1, -128, 0},
    {-kMax, kMax, -128, 0},

    // Zeros
    {0, 0, -128, 0},
    {0, kMax, -128, 0},

    // Well over boundary
    {1, 1, -129, 0},
    {1, 1, -200, 0},
    {kMax, -kMax, -200, 0},
};
// clang-format on

INSTANTIATE_TEST_SUITE_P(
    extreme_right_shift_cases, FixedMultiplicationTest,
    testing::ValuesIn(multiplication_extreme_right_shift_cases));

/*
  Extreme Positive (Left) Shift (desired_shift >= 64)

  Tests the `shift >= 64` branch, which should always return 0.
*/
// clang-format off
const MultiplicationParam multiplication_extreme_left_shift_cases[] = {
    // Boundary
    {1, 1, 64, kMax},
    {100, 200, 64, kMax},
    {kMax, kMax, 64, kMax},

    // Signs
    {-1, 1, 64, kMin},
    {-1, -1, 64, kMax},
    {-kMax, kMax, 64, kMin},

    // Zeros
    {0, 0, 64, 0},
    {0, kMax, 64, 0},

    // Well over boundary
    {1, 1, 65, kMax},
    {1, 1, 200, kMax},
    {kMax, -kMax, 200, kMin},
};
// clang-format on

INSTANTIATE_TEST_SUITE_P(
    extreme_left_shift_cases, FixedMultiplicationTest,
    testing::ValuesIn(multiplication_extreme_left_shift_cases));
#endif

/*
  Truncation.

  Tests that truncation rounds towards zero for both positive and negative
  cases.
*/
const MultiplicationParam multiplication_truncation_cases[] = {
    {3, 1, -1, 1},    //  1.5 ->  1
    {-3, 1, -1, -1},  // -1.5 -> -1
    {1, 1, -1, 0},    //  0.5 ->  0
    {-1, 1, -1, 0},   // -0.5 -> -0

    {3 << 10, 1, -11, 1},    //  1.5 ->  1
    {-3 << 10, 1, -11, -1},  // -1.5 -> -1
    {1 << 10, 1, -11, 0},    //  0.5 ->  0
    {-1 << 10, 1, -11, -0},  // -0.5 -> -0

    {-511, 1, -10, 0},   // -0.499... rounds to 0
    {-512, 1, -10, -1},  // -0.5      rounds to -1
    {511, 1, -10, 0},    //  0.499... rounds to 0
    {512, 1, -10, 1},    //  0.5      rounds to 1

    {kMin, (1LL << 62), -100, -(1LL << 25)},
    {kMax, (1LL << 62), -100, (1LL << 25)},
    {kMin, kMax, -100, -(1LL << 26)},
    {kMax, kMax, -100, (1LL << 26)},
    {kMin, kMin, -100, (1LL << 26)},

    // Final boss test vector.
    {kMin, kMin, -127, 1},
};

INSTANTIATE_TEST_SUITE_P(truncation_cases, FixedMultiplicationTest,
                         testing::ValuesIn(multiplication_truncation_cases));

// ----------------------------------------------------------------------------
// Division Tests
// ----------------------------------------------------------------------------

// Test Parameter
// ----------------------------------------------------------------------------

struct DivisionParam {
  s64 dividend;
  s64 divisor;
  int desired_shift;
  s64 expected_result;

  friend auto operator<<(std::ostream& out, const DivisionParam& src)
      -> std::ostream& {
    return out << "{" << src.dividend << ", " << src.divisor << ", "
               << src.desired_shift << ", " << src.expected_result << "}";
  }
};

// Test Fixture
// ----------------------------------------------------------------------------

struct FixedDivisionTest : testing::TestWithParam<DivisionParam> {
  const s64 dividend = GetParam().dividend;
  const s64 divisor = GetParam().divisor;
  const int desired_shift = GetParam().desired_shift;
  const s64 expected_result = GetParam().expected_result;
};

// Test Cases
// ----------------------------------------------------------------------------

TEST_P(FixedDivisionTest, divide) {
  const auto actual_result = curves_fixed_divide(
      0, dividend, 0, divisor, static_cast<unsigned int>(desired_shift));

  ASSERT_EQ(expected_result, actual_result);
}

const DivisionParam division_params[] = {
    // zero
    {0, 1, 0, 0},
    {0, -1, 0, 0},

    // simple positive
    {1, 1, 0, 1},
    {1, 1, 1, 2},

    // numerator < denominator
    {15, 26, 2, (15 << 2) / 26},
    {11, 89, 3, ((11 << 3) + 89 / 2) / 89},

    // numerator > denominator
    {26, 15, 2, ((26 << 2) + 15 / 2) / 15},
    {89, 11, 3, ((89 << 3) + 11 / 2) / 11},

    // unity
    {100, 100, 10, 1LL << 10},
    {1000, 1000, 20, 1LL << 20},

    // fixed point values
    {1447LL << 32, 13LL << 32, 32, ((1447LL << 32) + 13LL / 2) / 13LL},
    {13LL << 32, 1447LL << 32, 32, (13LL << 32) / 1447LL},

    // large positive values
    {1LL << 61, 1, 1, 1LL << 62},
    {1LL << 60, 1, 2, 1LL << 62},
    {1LL << 62, 2, 1, 1LL << 62},
    {1LL << 62, 4, 2, 1LL << 62},

    // large shifts
    {1, 1, 62, 1LL << 62},
    {1, 1, 63, kMax},
    {1, 2, 63, 1LL << 62},
    {1, 1LL << 10, 63, 1LL << 53},

    // small numerator / large denominator
    {1, 1LL << 62, 62, 1LL},
    {1, 1LL << 62, 63, 2},
    {10, 1LL << 62, 63, 20},

    // simple negatives
    {-1, 1, 0, -1},
    {1, -1, 0, -1},
    {-1, -1, 0, 1},
    {-100, 1, 0, -100},
    {100, -1, 0, -100},

    // negative / positive
    {-15, 26, 2, (-15 << 2) / 26},
    {-89, 11, 3, ((-89 << 3) - 11 / 2) / 11},

    // positive / negative
    {15, -26, 2, (15 << 2) / -26},
    {89, -11, 3, ((89 << 3) + 11 / 2) / -11},

    // negative / negative
    {-15, -26, 2, (-15 << 2) / -26},
    {-89, -11, 3, ((-89 << 3) - 11 / 2) / -11},

    // negative unity
    {-100, -100, 10, 1LL << 10},
    {-1000, -1000, 20, 1LL << 20},
    {100, -100, 10, -(1LL << 10)},
    {-100, 100, 10, -(1LL << 10)},

    // negative fixed point values
    {-1447LL << 32, 13LL << 32, 32, ((-1447LL << 32) - 13LL / 2) / 13LL},
    {1447LL << 32, -13LL << 32, 32, ((1447LL << 32) + 13LL / 2) / -13LL},
    {-1447LL << 32, -13LL << 32, 32, ((-1447LL << 32) - 13LL / 2) / -13LL},

    // large negative values
    {-(1LL << 61), 1, 1, -(1LL << 62)},
    {-(1LL << 60), 1, 2, -(1LL << 62)},
    {1LL << 61, -1, 1, -(1LL << 62)},

    // negative values with large shifts
    {-1, 1, 63, kMin},
    {-1, -1, 63, kMax},
    {-1, 1LL << 62, 63, -2},

    // kMax boundary
    {kMax, 1, 0, kMax},
    {kMax, -1, 0, -kMax},

    // various zeros
    {0, -100, 10, 0},
    {0, -(1LL << 62), 32, 0},
    {0, -1, 63, 0},

    // divisor == 0 error cases
    {0, 0, 0, 0},         // 0/0 = 0 (arbitrary choice)
    {0, 0, 32, 0},        // 0/0 with shift
    {1, 0, 0, kMax},      // positive/0 = kMax
    {100, 0, 10, kMax},   // positive/0 with shift
    {kMax, 0, 32, kMax},  // kMax/0
    {-1, 0, 0, kMin},     // negative/0 = kMin
    {-100, 0, 10, kMin},  // negative/0 with shift
    {kMin, 0, 32, kMin},  // kMin/0

    // shift >= 128 saturation cases
    {0, 1, 128, 0},    // 0 stays 0
    {0, -1, 128, 0},   // 0 stays 0 (negative divisor)
    {0, 100, 200, 0},  // 0 stays 0 (large shift)

    // positive dividend, positive divisor -> kMax
    {1, 1, 128, kMax},
    {100, 50, 128, kMax},
    {kMax, 1, 129, kMax},
    {1, kMax, 200, kMax},

    // positive dividend, negative divisor -> kMin
    {1, -1, 128, kMin},
    {100, -50, 128, kMin},
    {kMax, -1, 129, kMin},
    {1, -kMax, 200, kMin},

    // negative dividend, positive divisor -> kMin
    {-1, 1, 128, kMin},
    {-100, 50, 128, kMin},
    {kMin, 1, 129, kMin},
    {-1, kMax, 200, kMin},

    // negative dividend, negative divisor -> kMax
    {-1, -1, 128, kMax},
    {-100, -50, 128, kMax},
    {kMin, -1, 129, kMax},
    {-1, -kMax, 200, kMax},
};

INSTANTIATE_TEST_SUITE_P(division_params, FixedDivisionTest,
                         ValuesIn(division_params));
#endif

}  // namespace
}  // namespace curves
