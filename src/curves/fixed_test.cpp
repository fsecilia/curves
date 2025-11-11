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

}  // namespace
}  // namespace curves
