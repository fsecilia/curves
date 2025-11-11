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

// ----------------------------------------------------------------------------
// Fixed Test
// ----------------------------------------------------------------------------

struct FixedTest : Test {};

TEST_F(FixedTest, one_highest_precision) {
  const auto frac_bits = 62;
  const auto expected = 4611686018427387904ll;

  const auto actual = curves_const_1(frac_bits);

  ASSERT_EQ(expected, actual);
}

TEST_F(FixedTest, one_lowest_precision) {
  const auto frac_bits = 0;
  const auto expected = 1ll;

  const auto actual = curves_const_1(frac_bits);

  ASSERT_EQ(expected, actual);
}

// ----------------------------------------------------------------------------
// Constants Test
// ----------------------------------------------------------------------------

struct ConstantsTestParam {
  std::string name;
  curves_fixed_t (*constant_func)(unsigned int);
  unsigned int frac_bits;
  double expected_value;
  double tolerance;

  friend auto operator<<(std::ostream& out, const ConstantsTestParam& src)
      -> std::ostream& {
    return out << "{" << src.frac_bits << ", " << src.expected_value << ", "
               << src.tolerance << "}";
  }
};

struct FixedConstantsTest : public TestWithParam<ConstantsTestParam> {};

TEST_P(FixedConstantsTest, verify_constants) {
  const auto param = GetParam();

  const auto actual_fixed = param.constant_func(param.frac_bits);
  const auto one_fixed = curves_const_1(param.frac_bits);

  const auto actual_double = double(actual_fixed) / double(one_fixed);

  if (param.tolerance == 0.0) {
    ASSERT_DOUBLE_EQ(param.expected_value, actual_double);
  } else {
    ASSERT_NEAR(param.expected_value, actual_double, param.tolerance);
  }
}

const ConstantsTestParam constants_test_params[] = {
    // e
    {"e_high", curves_const_e, CURVES_E_FRAC_BITS, M_E, 0.0},
    {"e_medium", curves_const_e, CURVES_E_FRAC_BITS / 2, M_E, 6.0e-10},
    {"e_low", curves_const_e, 1, M_E, 2.2e-1},

    // ln(2)
    {"ln2_high", curves_const_ln2, CURVES_LN2_FRAC_BITS, std::log(2.0), 0.0},
    {"ln2_medium", curves_const_ln2, CURVES_LN2_FRAC_BITS / 2, std::log(2.0),
     4.3e-10},
    {"ln2_low", curves_const_ln2, 1, std::log(2.0), 2.0e-1},

    // pi
    {"pi_high", curves_const_pi, CURVES_PI_FRAC_BITS, M_PI, 0.0},
    {"pi_medium", curves_const_pi, CURVES_PI_FRAC_BITS / 2, M_PI, 1.3e-10},
    {"pi_low", curves_const_pi, 1, M_PI, 1.5e-1},
};

INSTANTIATE_TEST_SUITE_P(all_constants, FixedConstantsTest,
                         ValuesIn(constants_test_params),
                         [](const TestParamInfo<ConstantsTestParam>& info) {
                           return info.param.name;
                         });

// ----------------------------------------------------------------------------
// Integer Conversions Tests
// ----------------------------------------------------------------------------

struct IntegerConversionsTestParam {
  unsigned int frac_bits;
  int64_t integer_value;
  curves_fixed_t fixed_value;

  friend auto operator<<(std::ostream& out,
                         const IntegerConversionsTestParam& src)
      -> std::ostream& {
    return out << "{" << src.frac_bits << ", " << src.integer_value << ", "
               << src.fixed_value << "}";
  }
};

struct FixedConverstionsTestInteger
    : public TestWithParam<IntegerConversionsTestParam> {};

// Non-truncating
// ----------------------------------------------------------------------------

/*
  These are tests that don't truncate the fixed value, so they are the same
  in either direction.
*/
struct FixedConversionsTestIntegerNontruncating
    : public FixedConverstionsTestInteger {};

TEST_P(FixedConversionsTestIntegerNontruncating, to_fixed) {
  const auto param = GetParam();

  const auto expected = param.fixed_value;
  const auto actual =
      curves_fixed_from_integer(param.frac_bits, param.integer_value);

  ASSERT_EQ(expected, actual);
}

TEST_P(FixedConversionsTestIntegerNontruncating, to_integer) {
  const auto param = GetParam();

  const auto expected = param.integer_value;
  const auto actual =
      curves_fixed_to_integer(param.frac_bits, param.fixed_value);

  ASSERT_EQ(expected, actual);
}

// clang-format off
const IntegerConversionsTestParam
    nontruncating_integer_conversion_test_params[] = {
  // zero
  {1, 0, 0},
  {32, 0, 0},
  {63, 0, 0},

  // 1
  {1, 1, 1ll << 1},
  {32, 1, 1ll << 32},
  {62, 1, 1ll << 62},

  // 2
  {1, 2, 2ll << 1},
  {32, 2, 2ll << 32},
  {61, 2, 2ll << 61},

  // end of q15.48 range
  {1, (1ll << 15) - 1, ((1ll << 15) - 1) << 1},
  {24, (1ll << 15) - 1, ((1ll << 15) - 1) << 24},
  {48, (1ll << 15) - 1, ((1ll << 15) - 1) << 48},

  // end of q31.32 range
  {1, (1ll << 31) - 1, ((1ll << 31) - 1) << 1},
  {16, (1ll << 31) - 1, ((1ll << 31) - 1) << 16},
  {32, (1ll << 31) - 1, ((1ll << 31) - 1) << 32},

  // end of q47.16 range
  {1, (1ll << 47) - 1, ((1ll << 47) - 1) << 1},
  {8, (1ll << 47) - 1, ((1ll << 47) - 1) << 8},
  {16, (1ll << 47) - 1, ((1ll << 47) - 1) << 16},

  // end of q62.1 range
  {1, (1ll << 62) - 1, ((1ll << 62) - 1) << 1},

  // end of q63.0 range
  {0, INT64_MAX, INT64_MAX},

  // -1
  {1, -1, -1ll << 1},
  {32, -1, -1ll << 32},
  {62, -1, -1ll << 62},

  // -2
  {1, -2, -2ll << 1},
  {32, -2, -2ll << 32},
  {61, -2, -2ll << 61},

  // end of q15.48 range
  {1,  -1ll << 15, (-1ll << 15) << 1},
  {24, -1ll << 15, (-1ll << 15) << 24},
  {48, -1ll << 15, (-1ll << 15) << 48},

  // end of q31.32 range
  { 1, -1ll << 31, (-1ll << 31) << 1},
  {16, -1ll << 31, (-1ll << 31) << 16},
  {32, -1ll << 31, (-1ll << 31) << 32},

  // end of q47.16 range
  { 1, -1ll << 47, (-1ll << 47) << 1},
  { 8, -1ll << 47, (-1ll << 47) << 8},
  {16, -1ll << 47, (-1ll << 47) << 16},

  // end of q62.1 range
  { 1, -1ll << 62, (-1ll << 62) << 1},

  // end of negative q63.0 range
  {0, INT64_MIN, INT64_MIN},
};
// clang-format on

INSTANTIATE_TEST_SUITE_P(
    all_conversions, FixedConversionsTestIntegerNontruncating,
    ValuesIn(nontruncating_integer_conversion_test_params));

// Truncating/Flooring
// ----------------------------------------------------------------------------

/*
  These test that "truncation" is always towards negative infinity, both for
  positive and negative fixed-point values. It's conventionally called
  truncation instead of flooring because for positive values, it's the same,
  but all fixed point numbers truncate towards negative infinity, not zero, so
  the correct name is floor, not truncate.
*/
struct FixedConversionsTestIntegerTruncation
    : public FixedConverstionsTestInteger {};

TEST_P(FixedConversionsTestIntegerTruncation, truncation_is_flooring) {
  const auto param = GetParam();

  const auto expected = param.integer_value;
  const auto actual =
      curves_fixed_to_integer(param.frac_bits, param.fixed_value);

  ASSERT_EQ(expected, actual);
}

// clang-format off
const IntegerConversionsTestParam integer_truncation_test_params[] = {
  // nonzero positive: 2.75, truncates to 2, floors to 2
  {32, 2, 11811160064ll},

  // zero positive: 0.75, truncates to 0, floors to 0
  {32, 0, 3221225472ll},

  // zero negative: -0.25, truncates to 0, but floors to -1
  {32, -1, -1073741824ll},

  // nonzero negative: -2.25, truncates to -2, but floors to -3
  {32, -3, -9663676416ll},
};
// clang-format on

INSTANTIATE_TEST_SUITE_P(all_conversions, FixedConversionsTestIntegerTruncation,
                         ValuesIn(integer_truncation_test_params));

// ----------------------------------------------------------------------------
// Double Conversions Tests
// ----------------------------------------------------------------------------

// Double -> Fixed
// ----------------------------------------------------------------------------

struct DoubleConversionTestParam {
  unsigned int frac_bits;
  curves_fixed_t fixed_value;
  double double_value;
};

struct FixedConversionTestFixedFromDouble
    : TestWithParam<DoubleConversionTestParam> {};

TEST_P(FixedConversionTestFixedFromDouble, from_double) {
  const auto param = GetParam();
  const auto actual =
      curves_fixed_from_double(param.frac_bits, param.double_value);
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
  {0, -123, -123.45},
  {0, 123, 123.45},
  {0, 0, -0.9},
  {0, 0, 0.9},

  /*
    Normal values for frac_bits = 32:
      2ll << 32 -> 2
      1ll << 31 -> 0.5
      1ll << 30 -> 0.25
  */
  {32, (-2ll << 32) - ((1ll << 31) | (1ll << 30)), -2.75},
  {32, (2ll << 32) + ((1ll << 31) | (1ll << 30)), 2.75},

  /*
    The smallest bit at precision 32 is 1/2^32. 2^-33 is half of that, so
    the fixed point value we're generating here is actually
    2^-33*(1 << 32) = 0.5, which truncates to 0.

    These tests show it truncates to zero from both sides.
  */
  {32, 0, -std::ldexp(1.0, -33)},
  {32, 0, std::ldexp(1.0, -33)},

  /*
    Min and max representable values for frac_bits = 0.

    Ideally, we'd test against INT64_MAX, but it is a 63-bit number. A double
    only has 53 bits of precision, so it can't be stored precisely in a double.
    If we were to try to round trip it, the runtime would have to pick the
    closest representable double, which in this case, causes it to round up to
    2^64. The value in the double is then larger than INT64_MAX. Converting an
    out of range double to an integer is undefined behavior. In this specific
    case, on x64, converting back just happens to give the value INT64_MIN.
    That is about as different from the value we started with as could be, so
    the test fails.

    Instead, we use the largest round-trippable integer, which is:
      INT64_MAX - 1023 = (2^63 - 1) - (2^10 - 1) = 2^63 - 2^10

    INT64_MIN is representable, so we use it directly.
  */
  {0, INT64_MIN, static_cast<double>(INT64_MIN)},
  {0, INT64_MAX - 1023, static_cast<double>(INT64_MAX - 1023)},

  // Min and max representable values for frac_bits = 32
  {32, INT64_MIN, -static_cast<double>(1ll << 31)},
  {32, ((1ll << 31) - 1) << 32, static_cast<double>((1ll << 31) - 1)},

  // Min and max representable values for frac_bits = 62
  {62, INT64_MIN, -2.0},
  {62, 1ll << 62, 1.0},
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
      curves_fixed_to_double(param.frac_bits, param.fixed_value);
  ASSERT_DOUBLE_EQ(param.double_value, actual);
}

// clang-format off
const DoubleConversionTestParam to_double_params[] = {
  // frac_bits = 0 is just the original integers as doubles with no scaling.
  {0, 123, 123.0},
  {0, -456, -456.0},

  // frac_bits = 32, normal values with full precision
  {32, (2ll << 32) | (1ll << 31), 2.5},
  {32, (-3ll << 32) | (1ll << 31), -2.5},
  {32, 1, std::ldexp(1.0, -32)}, // 1/2^32
  {32, -1, -std::ldexp(1.0, -32)},

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
  {60, (1ll << 60) | (1ll << 0), 1.0}, // The 2^-60 part is lost
  {60, (1ll << 60) | (1ll << 6), 1.0}, // The 2^-54 part is lost
  {60, (1ll << 60) | (1ll << 7), 1.0 + std::ldexp(1.0, -53)}, // bit is kept
};
// clang-format on

INSTANTIATE_TEST_SUITE_P(all, FixedConversionTestFixedToDouble,
                         ValuesIn(to_double_params));

}  // namespace
}  // namespace curves
