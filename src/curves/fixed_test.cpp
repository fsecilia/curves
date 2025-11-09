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

struct fixed_test_t : Test {};

TEST_F(fixed_test_t, one_highest_precision) {
  const auto decimal_place = 62;
  const auto expected = 4611686018427387904ll;

  const auto actual = curves_const_one(decimal_place);

  ASSERT_EQ(expected, actual);
}

TEST_F(fixed_test_t, one_lowest_precision) {
  const auto decimal_place = 0;
  const auto expected = 1ll;

  const auto actual = curves_const_one(decimal_place);

  ASSERT_EQ(expected, actual);
}

// ----------------------------------------------------------------------------
// Constant Test
// ----------------------------------------------------------------------------

struct constant_test_param_t {
  std::string name;
  curves_fixed_t (*constant_func)(unsigned int);
  unsigned int decimal_place;
  double expected_value;
  double tolerance;

  friend auto operator<<(std::ostream& out, const constant_test_param_t& src)
      -> std::ostream& {
    return out << "{" << src.decimal_place << ", " << src.expected_value << ", "
               << src.tolerance << "}";
  }
};

struct fixed_constant_test_t : public TestWithParam<constant_test_param_t> {
  auto get_one(unsigned int decimal_place) const {
    return curves_const_one(decimal_place);
  }
};

TEST_P(fixed_constant_test_t, verify_constants) {
  const auto param = GetParam();

  const auto actual_fixed = param.constant_func(param.decimal_place);
  const auto one_fixed = get_one(param.decimal_place);

  const auto actual_double = double(actual_fixed) / double(one_fixed);

  if (param.tolerance == 0.0) {
    ASSERT_EQ(param.expected_value, actual_double);
  } else {
    ASSERT_NEAR(param.expected_value, actual_double, param.tolerance);
  }
}

constant_test_param_t constant_test_params[] = {
    // ln(2)
    {"ln2_high", curves_const_ln2, CURVES_LN_2_DECIMAL_PLACE, std::log(2.0),
     0.0},
    {"ln2_medium", curves_const_ln2, CURVES_LN_2_DECIMAL_PLACE / 2,
     std::log(2.0), 5.0e-10},
    {"ln2_low", curves_const_ln2, 1, std::log(2.0), 2.0e-1},
};

INSTANTIATE_TEST_SUITE_P(all_constants, fixed_constant_test_t,
                         ValuesIn(constant_test_params),
                         [](const TestParamInfo<constant_test_param_t>& info) {
                           return info.param.name;
                         });

}  // namespace
}  // namespace curves
