// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <curves/testing/test.hpp>
#include <curves/math/fixed.hpp>
#include <curves/math/io.hpp>
#include <curves/testing/isqrt.hpp>

namespace curves {
namespace {

TEST(isqrt_automated_tests, run_automated_tests) {
  // Small integer, exhaustive.
  //
  // Test every integer from 1 to 1000000.
  for (u64 x = 1; x < 1000000; ++x) {
    isqrt_test_verify_u64(x, 0, 32);
  }

  // Power of 2 seams.
  //
  // Test the transition points for all 64 bits.
  for (int i = 1; i < 63; ++i) {
    u64 power = 1ULL << i;

    // Test on, one above, and one below.
    isqrt_test_verify_u64(power - 1, 0, 32);
    isqrt_test_verify_u64(power, 0, 32);
    isqrt_test_verify_u64(power + 1, 0, 32);

    // Also verify with odd fractional bits to test the parity shift.
    isqrt_test_verify_u64(power, 15, 32);
  }

  // Monotonicity sweep.
  //
  // Pick a start, check the next 1000 numbers for strictly decreasing
  // order. Do this 1000 times.
  for (u64 i = 0; i < 1000; ++i) {
    u64 start = 7001 * i + 1;
    u64 x = start;
    u64 prev = curves_fixed_isqrt(x++, 0, 32);
    for (u64 j = 0; j < 1000; ++j) {
      u64 cur = curves_fixed_isqrt(x, 0, 32);
      ASSERT_GE(prev, cur) << "Monotonicity violated at " << x;
      ++x;
      prev = cur;
    }
  }
}

}  // namespace
}  // namespace curves
