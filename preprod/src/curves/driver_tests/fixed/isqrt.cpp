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

// ----------------------------------------------------------------------------
// curves_fixed_isqrt()
// ----------------------------------------------------------------------------

struct ISqrtParam {
  u64 value;
  unsigned int frac_bits;
  unsigned int output_frac_bits;
  u64 tolerance;
  u64 expected_result;

  friend auto operator<<(std::ostream& out, const ISqrtParam& src)
      -> std::ostream& {
    return out << "{" << src.value << ", " << src.frac_bits << ", "
               << src.output_frac_bits << ", " << src.tolerance << ", "
               << src.expected_result << " } ";
  }
};

struct ISqrtTest : TestWithParam<ISqrtParam> {};

TEST_P(ISqrtTest, expected_result) {
  const auto expected_result = GetParam().expected_result;
  const auto expected_delta = GetParam().tolerance;

  const auto actual_result = curves_fixed_isqrt(
      GetParam().value, GetParam().frac_bits, GetParam().output_frac_bits);

  const auto actual_delta = actual_result > expected_result
                                ? actual_result - expected_result
                                : expected_result - actual_result;
  EXPECT_LE(actual_delta, expected_delta)
      << "Input:     " << GetParam().value << "@Q" << GetParam().frac_bits
      << "\nExpected:  " << expected_result << "@Q"
      << GetParam().output_frac_bits << "\nActual:    " << actual_result << "@Q"
      << GetParam().output_frac_bits << "\nDiff:      " << actual_delta
      << "\nTolerance: " << expected_delta;
}

const ISqrtParam isqrt_smoke_test[] = {
    // Identity Case
    // isqrt(1.0) == 1.0.
    // Basic baseline check.
    {1LL << 30, 30, 30, 0, 1LL << 30},

    // The "Problem" Case (High Precision Under-unity result)
    // isqrt(2.0) at Q61. Result is ~0.707.
    // This fails if internal precision doesn't have guard bits for RNE.
    // Expected: round(2^61/sqrt(2))
    {2LL << 61, 61, 61, 148, 1630477228166597777ULL},

    // The "Overflow" Risk Case (High Precision Over-unity result)
    // isqrt(0.5) at Q61. Result is sqrt(2) (~1.414).
    // Expected: round(2^61/sqrt(0.5))
    {1LL << 60, 61, 61, 295, 3260954456333195553ULL},

    // Pure Integer Input
    // isqrt(100) == 0.1
    // Checks standard integer handling and large rescaling (Q0 -> Q60).
    // Expected: round(2^60/sqrt(100))
    {100, 0, 60, 1, 115292150460684698ULL},

    // Irrational Non-Power-of-2
    // isqrt(3.0) at Q60.
    // Checks rounding logic on standard messy numbers.
    // Expected: round(2^60/sqrt(3))
    {3LL << 60, 60, 60, 135892519, 665639541039271463ULL},

    // Large Upscale (Small Input)
    // Expected: round(2^30/sqrt(trunc(0.001*2^30)/2^30))
    {(s64)(0.001 * (1LL << 30)), 30, 30, 0, 33954710857LL},

    // The "Bad Guess"
    // Input: 6.0.
    // Logic: log2(6) is 2. The guess logic estimates 1/sqrt(2^2) = 0.5
    // Reality: 1/sqrt(6) = 0.4082.
    // This forces the algorithm to bridge a massive gap (0.25 -> 0.408) purely
    // via NR steps.
    // If it converges in 6 steps here, it converges everywhere.
    // Expected: round(2^60/sqrt(6))
    {6LL << 60, 60, 60, 51056511, 470678233243713536ULL},

    // The "Precision Cliff" (Alternating Bits)
    // Input: 0x5...5 (0.333...). High entropy bit pattern.
    // The mathematical result lands at ...025.5002.
    // This is the ultimate test of your Fused Update and RNE.
    // If you have ANY bias, you will snap to ...025. If correct, ...026.
    // Expected: round(2^60 / sqrt(0x555... * 2^-60))
    {0x5555555555555555LL, 60, 60, 101919389, 499229655779453597LL},

    // THE FLOOR (Flat Slope)
    // Input: S64_MAX (in Q0). This is the largest possible input.
    // Math: 1 / sqrt(2^63 - 1) approx 1 / 3,037,000,499.
    // Result is very small. This tests if we lose bits when y is tiny.
    // Expected: 2^60 * (1/sqrt(2^63-1))
    {S64_MAX, 0, 60, 0, 379625062LL},

    // THE CEILING (Steep Slope)
    // Input: 1 (in Q0). This is x=1.0.
    // Math: 1 / sqrt(1). Result 1.0.
    // This checks the Identity at maximum integer magnitude.
    {1, 0, 60, 104, 1LL << 60},

    // THE "ALMOST" POWER OF 2 (CLZ Stress)
    // Input: (1<<62) - 1. This is all 1s in the high bits.
    // It has the same CLZ as (1<<62), but a vastly different mantissa.
    // This breaks algorithms that rely too heavily on the exponent guess.
    // Math: 1 / sqrt(4.0 - epsilon) -> Just slightly > 0.5
    // 0.500000000000000000135 * 2^60
    // Expected: round(2^60/sqrt((2^62 - 1)/2^60))
    {(1LL << 62) - 1, 60, 60, 195811, 576460752303423488LL},

    // THE SATURATION RISK (Output Overflow)
    // Input: Smallest normalized x in Q30 (value 1).
    // Math: x = 2^-30. 1/sqrt(x) = 2^15 = 32768.
    // We request Output Q50. Result: 32768 * 2^50 = 2^65.
    // This MUST saturate to U64_MAX.
    {1, 30, 50, U64_MAX, U64_MAX},

    // THE UNDERFLOW RISK (Output Vanishing)
    // Input: Large x in Q0 (2^60).
    // Math: 1/sqrt(2^60) = 2^-30.
    // We request Output Q20. Result: 2^-30 * 2^20 = 2^-10.
    // This is less than 1 integer unit. It should round to 0.
    {1LL << 60, 0, 20, 0, 0},

    // MAX MOUSE VECTOR
    // round(2^32/sqrt(2*(2^15 - 1)*(2^15 - 1)))
    {2 * ((1LL << 15) - 1) * ((1LL << 15) - 1), 0, 32, 0, 92685},

    // MIN MOUSE VECTOR
    // round(2^32/sqrt(2*2^30))
    {(2LL << 30), 0, 32, 0, 92682},
};
INSTANTIATE_TEST_SUITE_P(smoke_tests, ISqrtTest, ValuesIn(isqrt_smoke_test));

}  // namespace
}  // namespace curves
