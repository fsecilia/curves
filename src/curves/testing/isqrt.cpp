// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "isqrt.hpp"
#include <curves/io.hpp>
#include <ostream>

namespace curves {

/*
  Verifies y = 1/sqrt(x) via y^2 = 1/x.

  Since y is a fixed-point approximation, it contains quantization error
  'e' (max 0.5). Squaring propagates this error via binomial expansion:

    (y + e)^2 = y^2 + 2ye + e^2

  The term '2ye' dominates. With worst-case e = 0.5:

    Error ~= 2 * y * 0.5
    Error ~= y

  Therefore, the check tolerance must be at least y.
*/
struct isqrt_test_expected_result create_isqrt_test_expected_result(
    struct isqrt_test_vector test_vector) {
  struct isqrt_test_expected_result result;

  result.test_vector = test_vector;
  if (test_vector.x == 0) {
    result.y = U64_MAX;
    result.expected = CURVES_U128_MAX;
    result.actual = CURVES_U128_MAX;
    result.tolerance = CURVES_U128_MAX;
    result.diff = CURVES_U128_MAX;
    return result;
  }

  // Get nominal result from sut.
  result.y = curves_fixed_isqrt(test_vector.x, test_vector.frac_bits,
                                test_vector.output_frac_bits);

  // Calculate y^2.
  result.actual = (u128)result.y * result.y;
  unsigned int actual_frac_bits = 2 * test_vector.output_frac_bits;

  // Calculate 1/x.
  result.expected = ((u128)1 << 127) / test_vector.x;
  unsigned int expected_frac_bits = 127 - test_vector.frac_bits;

  // Align larger binary point to smaller using shr_rne.
  u64 max_error = result.y;
  int shift = (int)actual_frac_bits - (int)expected_frac_bits;
  if (shift > 0) {
    result.actual =
        __curves_fixed_shr_rne_u128(result.actual, (unsigned int)shift);
    max_error >>= shift;
  } else if (shift < 0) {
    result.expected =
        __curves_fixed_shr_rne_u128(result.expected, (unsigned int)-shift);
  }

  // Choose the larger tolerance between relative error and max error.
  result.tolerance = result.expected >> 11;
  if (result.tolerance < max_error) {
    result.tolerance = max_error;
  }
  result.diff = (result.expected < result.actual)
                    ? (result.actual - result.expected)
                    : (result.expected - result.actual);

  return result;
}

void isqrt_test_verify_result(
    struct isqrt_test_expected_result expected_result) {
  // u128 doesn't print to a gtest Message using our ostream inserter. This is
  // the least bad workaround to print the contents when the test fails.
  std::ostringstream out;
  if (expected_result.diff > expected_result.tolerance) {
    out << "x:         " << expected_result.test_vector.x << "@Q"
        << expected_result.test_vector.frac_bits
        << "\ny:         " << expected_result.y << "@Q"
        << expected_result.test_vector.output_frac_bits
        << "\nExpected:  " << expected_result.expected << "@Q"
        << "\nActual:    " << expected_result.actual << "@Q"
        << expected_result.test_vector.output_frac_bits
        << "\nDiff:      " << expected_result.diff
        << "\nTolerance: " << expected_result.tolerance;
  }

  ASSERT_LE(expected_result.diff, expected_result.tolerance) << out.str();
}

void isqrt_test_verify_test_vector(struct isqrt_test_vector test_vector) {
  isqrt_test_verify_result(create_isqrt_test_expected_result(test_vector));
}

void isqrt_test_verify_u64(u64 x, unsigned int frac_bits,
                           unsigned int output_frac_bits) {
  struct isqrt_test_vector test_vector{x, frac_bits, output_frac_bits};
  isqrt_test_verify_test_vector(test_vector);
}

}  // namespace curves
