// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <curves/testing/test.hpp>
#include <curves/fixed.hpp>

namespace curves {
namespace {

struct Exp2Param {
  s64 x;
  unsigned int x_frac_bits;
  unsigned int output_frac_bits;
  u64 expected_result;

  friend auto operator<<(std::ostream& out, const Exp2Param& src)
      -> std::ostream& {
    return out << "{" << src.x << ", " << src.x_frac_bits << ", "
               << src.output_frac_bits << ", " << src.expected_result << "}";
  }
};

struct Exp2Test : TestWithParam<Exp2Param> {};

TEST_P(Exp2Test, expected_result) {
  const auto expected = GetParam().expected_result;

  const auto actual = curves_fixed_exp2(GetParam().x, GetParam().x_frac_bits,
                                        GetParam().output_frac_bits);

  ASSERT_EQ(expected, actual);
}

const Exp2Param exp2_smoke_tests[] = {
    {4, 0, 0, 16},
    {5, 1, 16, 370728},

    // -------------------------------------------------------------------------
    // 1. The Classics (Roots & Inverses)
    // -------------------------------------------------------------------------

    // 2^0.5 (Sqrt 2) -> 1.41421356...
    // Input: 0.5 (Q32) -> 2147483648
    // Output: Q16 (x 65536) -> 92681.9... -> RNE Rounds Up -> 92682
    {2147483648, 32, 16, 92682},

    // 2^-0.5 (1 / Sqrt 2) -> 0.70710678...
    // Input: -0.5 (Q32) -> -2147483648
    // Output: Q16 (x 65536) -> 46340.95... -> RNE Rounds Up -> 46341
    {-2147483648, 32, 16, 46341},

    // -------------------------------------------------------------------------
    // 2. Integer Boundaries (Exact Powers of 2)
    // -------------------------------------------------------------------------

    // 2^0 = 1.0
    // Input: 0 (Q16)
    // Output: Q16 -> 65536
    {0, 16, 16, 65536},

    // 2^-10 = 0.0009765625
    // Input: -10 (Q0)
    // Output: Q16 -> 0.000976... * 65536 = 64.0 (Exact)
    {-10, 0, 16, 64},

    // 2^16 = 65536
    // Input: 16 (Q0)
    // Output: Q0 -> 65536
    {16, 0, 0, 65536},

    // -------------------------------------------------------------------------
    // 3. RNE Torture Test (The "0.5" Case)
    // -------------------------------------------------------------------------
    // 2^-1 = 0.5
    // In RNE, 0.5 rounds to the nearest EVEN integer.
    // 0 is even, 1 is odd. 0.5 should round DOWN to 0.

    // Input: -1 (Q0)
    // Output: Q0 (scale=1) -> Result 0.5 -> Rounds to 0
    {-1, 0, 0, 0},

    // Counter-test: 2^log2(1.5) approx 0.5849...
    // Let's just use 2^0 = 1.0.
    // If we request Q-1 output (div 2), we get 0.5.
    // 0.5 (0.1 binary) -> Nearest Even (0) -> 0.
    // This is hard to test with just exp2, so we rely on the rescaler tests
    // mostly.

    // -------------------------------------------------------------------------
    // 4. Saturation & Underflow
    // -------------------------------------------------------------------------

    // Saturation: 2^64
    // Input: 64 (Q0)
    // Output: Q0 -> Should be huge. Saturates to U64_MAX.
    {64, 0, 0, U64_MAX},

    // Saturation: 2^10 = 1024
    // Output: Q55. 1024 * 2^55 = 2^10 * 2^55 = 2^65.
    // Exceeds u64 (2^64). Should saturate.
    {10, 0, 55, U64_MAX},

    // Underflow: 2^-65
    // Input: -65 (Q0)
    // Output: Q0 -> Effectively 0.
    {-65, 0, 0, 0},

    // -------------------------------------------------------------------------
    // 5. The "Mouse Flick" (Reasonable Real World)
    // -------------------------------------------------------------------------
    // User moves mouse fast. x = 3.14159... (Q16 input)
    // Input: 205887 (3.14159 * 65536)
    // Value: 2^3.14159... = 8.8249...
    // Output: Q16 -> 8.8249 * 65536 = 578353.4 -> Rounds Down -> 578353
    {205887, 16, 16, 578351},
};
INSTANTIATE_TEST_SUITE_P(smoke_tests, Exp2Test, ValuesIn(exp2_smoke_tests));

}  // namespace
}  // namespace curves
