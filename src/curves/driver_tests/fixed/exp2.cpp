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
  u64 tolerance;
  u64 expected_result;

  friend auto operator<<(std::ostream& out, const Exp2Param& src)
      -> std::ostream& {
    return out << "{" << src.x << ", " << src.x_frac_bits << ", "
               << src.output_frac_bits << ", " << src.tolerance << ", "
               << src.expected_result << "}";
  }
};

struct Exp2Test : TestWithParam<Exp2Param> {};

TEST_P(Exp2Test, expected_result) {
  const auto expected_result = GetParam().expected_result;
  const auto expected_delta = GetParam().tolerance;

  const auto actual_result = curves_fixed_exp2(
      GetParam().x, GetParam().x_frac_bits, GetParam().output_frac_bits);

  const auto actual_delta = actual_result > expected_result
                                ? actual_result - expected_result
                                : expected_result - actual_result;
  EXPECT_LE(actual_delta, expected_delta)
      << "Input:     " << GetParam().x << "@Q" << GetParam().x_frac_bits
      << "\nExpected:  " << expected_result << "@Q"
      << GetParam().output_frac_bits << "\nActual:    " << actual_result << "@Q"
      << GetParam().output_frac_bits << "\nDiff:      " << actual_delta
      << "\nTolerance: " << expected_delta;
}

const Exp2Param exp2_smoke_tests[] = {
    {4, 0, 0, 0, 16},
    {5, 1, 16, 0, 370728},

    // Roots & Inverses

    // 2^0.5 (Sqrt 2) -> 1.41421356...
    // Input: 0.5 (Q32) -> 2147483648
    // Output: Q16 (x 65536) -> 92681.9... -> RNE Rounds Up -> 92682
    {2147483648, 32, 16, 0, 92682},

    // 2^-0.5 (1 / Sqrt 2) -> 0.70710678...
    // Input: -0.5 (Q32) -> -2147483648
    // Output: Q16 (x 65536) -> 46340.95... -> RNE Rounds Up -> 46341
    {-2147483648, 32, 16, 0, 46341},

    // Integer Boundaries (Exact Powers of 2)

    // 2^0 = 1.0
    // Input: 0 (Q16)
    // Output: Q16 -> 65536
    {0, 16, 16, 0, 65536},

    // 2^-10 = 0.0009765625
    // Input: -10 (Q0)
    // Output: Q16 -> 0.000976... * 65536 = 64.0 (Exact)
    {-10, 0, 16, 0, 64},

    // 2^16 = 65536
    // Input: 16 (Q0)
    // Output: Q0 -> 65536
    {16, 0, 0, 0, 65536},

    // RNE Torture Test (0.5)

    // 2^-1 = 0.5
    // In RNE, 0.5 rounds to the nearest EVEN integer.
    // 0 is even, 1 is odd. 0.5 should round DOWN to 0.
    // Input: -1 (Q0)
    // Output: Q0 (scale=1) -> Result 0.5 -> Rounds to 0
    {-1, 0, 0, 0, 0},

    // 2^log(1.5) in various precisions
    //
    // Inputs:
    // round(log2(1.5)*2^24) = 9814042
    // round(log2(1.5)*2^32) = 2512394810
    // round(log2(1.5)*2^48) = 164652306267095
    //
    // output: round(2^(input/2^x_frac_bits)*2^output_frac_bits)
    {9814042ULL, 24, 24, 0, 25165824ULL},
    {9814042ULL, 24, 32, 0, 6442450884ULL},
    {9814042ULL, 24, 48, 0, 422212461115022ULL},
    {2512394810ULL, 32, 24, 0, 25165824ULL},
    {2512394810ULL, 32, 32, 0, 6442450944ULL},
    {2512394810ULL, 32, 48, 0, 422212465067092ULL},
    {164652306267095ULL, 48, 24, 0, 25165824ULL},
    {164652306267095ULL, 48, 32, 0, 6442450944ULL},
    {164652306267095ULL, 48, 48, 0, 422212465065984ULL},

    // Saturation & Underflow

    // Saturation: 2^64
    // Input: 64 (Q0)
    // Output: Q0 -> Should be huge. Saturates to U64_MAX.
    {64, 0, 0, 0, U64_MAX},

    // Saturation: 2^10 = 1024
    // Output: Q55. 1024 * 2^55 = 2^10 * 2^55 = 2^65.
    // Exceeds u64 (2^64). Should saturate.
    {10, 0, 55, 0, U64_MAX},

    // Underflow: 2^-65
    // Input: -65 (Q0)
    // Output: Q0 -> Effectively 0.
    {-65, 0, 0, 0, 0},

    // Mouse Flick
    // User moves mouse fast. x = 3.14159... (Q16 input)
    // Input: 205887 (3.14159 * 65536)
    // Value: 2^3.14159... = 8.8249...
    // Output: Q16 -> 8.8249 * 65536 = 578351.201 -> Rounds Down -> 578351
    // From wolfram alpha:
    // round(2^(round(3.14159 * 65536)/65536)*65536) = 578351
    {205887, 16, 16, 0, 578351},
};
INSTANTIATE_TEST_SUITE_P(smoke_tests, Exp2Test, ValuesIn(exp2_smoke_tests));

}  // namespace
}  // namespace curves
