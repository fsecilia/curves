// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <cmath>
#include <curves/test.hpp>

extern "C" {
#include <curves/driver/fixed.h>
}  // extern "C"

namespace curves {
namespace {

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

TEST_F(fixed_test_t, ln2_highest_precision) {
  const auto decimal_place = CURVES_LN_2_DECIMAL_PLACE;
  const auto expected = std::log(2.0);

  const auto actual = double(curves_const_ln2(decimal_place)) /
                      double(curves_const_one(decimal_place));

  ASSERT_EQ(expected, actual);
}

TEST_F(fixed_test_t, ln2_medium_precision) {
  const auto decimal_place = CURVES_LN_2_DECIMAL_PLACE / 2;
  const auto expected = std::log(2.0);

  const auto actual = double(curves_const_ln2(decimal_place)) /
                      double(curves_const_one(decimal_place));

  ASSERT_NEAR(expected, actual, 5.0e-10);
}

TEST_F(fixed_test_t, ln2_lowest_precision) {
  const auto decimal_place = 1;
  const auto expected = std::log(2.0);

  const auto actual = double(curves_const_ln2(decimal_place)) /
                      double(curves_const_one(decimal_place));

  ASSERT_NEAR(expected, actual, 2.0e-1);
}

}  // namespace
}  // namespace curves
