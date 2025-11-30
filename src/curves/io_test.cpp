// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "io.hpp"
#include <curves/test/test.hpp>
#include <sstream>
#include <string>

namespace curves {
namespace {

// ----------------------------------------------------------------------------
// u128
// ----------------------------------------------------------------------------

struct FixedIoU128TestParam {
  u128 number;
  std::string string;
};

struct FixedIoU128Test : TestWithParam<FixedIoU128TestParam> {};

TEST_P(FixedIoU128Test, result) {
  const auto expected = GetParam().string;

  std::ostringstream out;
  out << GetParam().number;
  const auto actual = out.str();

  ASSERT_EQ(expected, actual);
}

const FixedIoU128TestParam u128_test_params[] = {
    {0, "0"},
    {1, "1"},
    {9, "9"},
    {10, "10"},
    {11, "11"},
    {99, "99"},
    {100, "100"},
    {101, "101"},
    {S64_MAX, "9223372036854775807"},
    {U64_MAX, "18446744073709551615"},
    {CURVES_S128_MAX, "170141183460469231731687303715884105727"},
    {CURVES_U128_MAX, "340282366920938463463374607431768211455"},
};
INSTANTIATE_TEST_SUITE_P(all_cases, FixedIoU128Test,
                         ValuesIn(u128_test_params));

// ----------------------------------------------------------------------------
// s128
// ----------------------------------------------------------------------------

struct FixedIoS128TestParam {
  s128 number;
  std::string string;
};

struct FixedIoS128Test : TestWithParam<FixedIoS128TestParam> {};

TEST_P(FixedIoS128Test, result) {
  const auto expected = GetParam().string;

  std::ostringstream out;
  out << GetParam().number;
  const auto actual = out.str();

  ASSERT_EQ(expected, actual);
}

const FixedIoS128TestParam s128_test_params[] = {
    {CURVES_S128_MIN, "-170141183460469231731687303715884105728"},
    {S64_MIN, "-9223372036854775808"},
    {-101, "-101"},
    {-100, "-100"},
    {-99, "-99"},
    {-11, "-11"},
    {-10, "-10"},
    {-9, "-9"},
    {-1, "-1"},

    {0, "0"},

    {1, "1"},
    {9, "9"},
    {10, "10"},
    {11, "11"},
    {99, "99"},
    {100, "100"},
    {101, "101"},
    {S64_MAX, "9223372036854775807"},
    {U64_MAX, "18446744073709551615"},
    {CURVES_S128_MAX, "170141183460469231731687303715884105727"},
};
INSTANTIATE_TEST_SUITE_P(all_cases, FixedIoS128Test,
                         ValuesIn(s128_test_params));

}  // namespace
}  // namespace curves
