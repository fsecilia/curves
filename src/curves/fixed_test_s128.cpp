// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia

  This file contains tests for s128 versions of functions that also have an s128
  version.
*/

#include "fixed.hpp"
#include "fixed_io.hpp"
#include <curves/test.hpp>

#define S128_MAX CURVES_S128_MAX
#define S128_MIN CURVES_S128_MIN

namespace curves {
namespace {

// ----------------------------------------------------------------------------
// curves_saturate_s128
// ----------------------------------------------------------------------------

TEST(curves_saturate_s128, negative) {
  ASSERT_EQ(S128_MIN, curves_saturate_s128(false));
}

TEST(curves_saturate_s128, positive) {
  ASSERT_EQ(S128_MAX, curves_saturate_s128(true));
}

// ----------------------------------------------------------------------------
// __curves_fixed_rescale_error_s128
// ----------------------------------------------------------------------------

struct FixedRescaleErrorS128TestParam {
  s128 value;
  unsigned int frac_bits;
  unsigned int output_frac_bits;
  s128 expected_result;

  friend auto operator<<(std::ostream& out,
                         const FixedRescaleErrorS128TestParam& src)
      -> std::ostream& {
    return out << "{" << src.value << ", " << src.frac_bits << ", "
               << src.output_frac_bits << ", " << src.expected_result << "}";
  }
};

struct FixedRescaleErrorS128Test
    : TestWithParam<FixedRescaleErrorS128TestParam> {};

TEST_P(FixedRescaleErrorS128Test, expected_result) {
  const auto actual_result = __curves_fixed_rescale_error_s128(
      GetParam().value, GetParam().frac_bits, GetParam().output_frac_bits);

  ASSERT_EQ(GetParam().expected_result, actual_result);
}

/*
  Tests zero-value inputs. These always return zero regardless of shift
  direction or precision, since zero can't overflow.
*/
const FixedRescaleErrorS128TestParam rescale_error_s128_zero_values[] = {
    {0, 0, 0, 0},  // All fractional bits zero
    {0, 1, 1, 0},  // No shift, nonzero fractional bits
    {0, 1, 0, 0},  // Right shift
    {0, 0, 1, 0},  // Left shift
};
INSTANTIATE_TEST_SUITE_P(zero_values, FixedRescaleErrorS128Test,
                         ValuesIn(rescale_error_s128_zero_values));

/*
  Tests right shift cases, output_frac_bits < frac_bits. The error handler
  returns zero for right shifts regardless of the input value, since right
  shifts reduce magnitude and cannot cause overflow.
*/
const FixedRescaleErrorS128TestParam rescale_error_s128_shr[] = {
    {-1, 1, 0, 0},
    {1, 1, 0, 0},
};
INSTANTIATE_TEST_SUITE_P(right_shifts, FixedRescaleErrorS128Test,
                         ValuesIn(rescale_error_s128_shr));

/*
  Tests no-shift cases, output_frac_bits == frac_bits, with non-zero values.
  When an invalid number of fractional bits cause the error handler to be
  called with no shift required, non-zero values saturate based on their sign.
*/
const FixedRescaleErrorS128TestParam rescale_error_s128_no_shift_sat[] = {
    {1, 0, 0, S128_MAX},   // Positive saturates to max
    {-1, 0, 0, S128_MIN},  // Negative saturates to min
};
INSTANTIATE_TEST_SUITE_P(no_shift_saturation, FixedRescaleErrorS128Test,
                         ValuesIn(rescale_error_s128_no_shift_sat));

/*
  Tests left shift cases, output_frac_bits > frac_bits, with non-zero values.
  Left shifts that trigger the error handler cause saturation based on sign.
  Tests include both regular values and boundary values at S128_MAX/S128_MIN.
*/
const FixedRescaleErrorS128TestParam rescale_error_s128_shl_sat[] = {
    {1, 0, 1, S128_MAX},         // Positive regular value
    {-1, 0, 1, S128_MIN},        // Negative regular value
    {S128_MAX, 0, 1, S128_MAX},  // Positive boundary value
    {S128_MIN, 0, 1, S128_MIN},  // Negative boundary value
};
INSTANTIATE_TEST_SUITE_P(left_shift_saturation, FixedRescaleErrorS128Test,
                         ValuesIn(rescale_error_s128_shl_sat));

}  // namespace
}  // namespace curves
