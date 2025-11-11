// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <cmath>
#include <curves/test.hpp>
#include <string>

extern "C" {
#include <curves/driver/fixed.h>
}  // extern "C"

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
    ASSERT_EQ(param.expected_value, actual_double);
  } else {
    ASSERT_NEAR(param.expected_value, actual_double, param.tolerance);
  }
}

ConstantsTestParam constants_test_params[] = {
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
IntegerConversionsTestParam nontruncating_integer_conversion_test_params[] = {
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
IntegerConversionsTestParam integer_truncation_test_params[] = {
  // nonzero positive: 2.75, truncates to 2, floors to 2
  {32, 2, 11811160064ll},

  // zero positive: 0.75, truncates to 0, floors to 0
  {32, 0, 3221225472ll},

  // zero negative: -0.25, truncates to 0, but floors to -1
  {32, -1, -1073741824ll},

  // nonzero negative: -2.25, truncates to -2, but floors to -3
  {32, -3, -9663676416ll},
};
// clang_format on

INSTANTIATE_TEST_SUITE_P(all_conversions, FixedConversionsTestIntegerTruncation,
                         ValuesIn(integer_truncation_test_params));

}  // namespace
}  // namespace curves
