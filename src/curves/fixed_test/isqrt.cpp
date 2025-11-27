// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <curves/test.hpp>
#include <curves/fixed.hpp>
#include <limits>

namespace curves {
namespace {

// ----------------------------------------------------------------------------
// curves_fixed_isqrt()
// ----------------------------------------------------------------------------

// #define trace(x) std::cout << #x << " = " << x << std::endl
// #define trace(x)
static inline u64 curves_fixed_isqrt(u64 x, unsigned int frac_bits,
                                     unsigned int output_frac_bits) {
  int x_bits, y_bits, x_frac_bits_odd, y_int_bits, y_frac_bits,
      initial_guess_exp;
  int x_frac_bits = (int)frac_bits;
  u64 result;
  u128 y, yy, xyy, term, y_new;

  if (x == 0) return U64_MAX;

  x_bits = (int)curves_log2_u64(x);

  // x_frac_bits must be even.
  x_frac_bits_odd = x_frac_bits & 1;
  x_frac_bits += x_frac_bits_odd;
  x_bits += x_frac_bits_odd;
  x <<= x_frac_bits_odd;

  // Linearly reduce precision of x when it interferes with yy.
  if (x_bits > 32) {
    int reduction = x_bits - 32;

    // Reduction must be even.
    int reduction_odd = reduction & 1;
    reduction += reduction_odd;

    x_bits -= reduction;
    x_frac_bits -= reduction;
    x >>= reduction;
  }

  y_bits = (126 - x_bits) >> 1;
  y_int_bits = ((-x_bits) >> 1) + 1;
  if (y_int_bits < 0) y_int_bits = 0;
  y_frac_bits = y_bits - y_int_bits;

  initial_guess_exp = (-x_bits >> 1) + y_frac_bits;

  y = 1LL << initial_guess_exp;
  // y = (y * 15) >> 4;  // Start at 15/16ths, so we always underestimate.

  for (int i = 0; i < 16; ++i) {
    yy = (u128)y * (u128)y;
    xyy = (u128)x * yy;
    term = ((u128)3 << y_frac_bits) - (xyy >> y_frac_bits);
    y_new = (y * term) >> (y_frac_bits + 1);
    if (y == y_new) {
      std::cout << "converged after " << i << std::endl;
      break;
    }
    y = y_new;
  }

  result = curves_narrow_u128_u64(curves_fixed_rescale_u128(
      y, (unsigned int)(y_frac_bits - (x_frac_bits >> 1)), output_frac_bits));

  return result;
}

struct IsqrtParam {
  u64 value;
  unsigned int frac_bits;
  unsigned int output_frac_bits;
  u64 tolerance;
  u64 expected_result;

  friend auto operator<<(std::ostream& out, const IsqrtParam& src)
      -> std::ostream& {
    return out << "{" << src.value << ", " << src.frac_bits << ", "
               << src.output_frac_bits << ", " << src.tolerance << ", "
               << src.expected_result << " } ";
  }
};

struct IsqrtTest : TestWithParam<IsqrtParam> {};

TEST_P(IsqrtTest, expected_result) {
  const auto expected_result = GetParam().expected_result;
  const auto expected_delta = GetParam().tolerance;

  const auto actual_result = curves_fixed_isqrt(
      GetParam().value, GetParam().frac_bits, GetParam().output_frac_bits);

  const auto actual_delta = actual_result > expected_result
                                ? actual_result - expected_result
                                : expected_result - actual_result;
  EXPECT_LE(actual_delta, expected_delta)
      << "Input:     " << GetParam().value << "@" << GetParam().frac_bits
      << "\nExpected:  " << expected_result << "@"
      << GetParam().output_frac_bits << "\nActual:    " << actual_result
      << "\nDiff:      " << actual_delta << "\nTolerance: " << expected_delta;
}

#if 1
auto isqrt_gamut_expected_result(unsigned int value_bits,
                                 unsigned int frac_bits,
                                 unsigned int output_frac_bits) -> u64 {
  return static_cast<u64>(
      std::nearbyint(static_cast<double>(1ULL << output_frac_bits) *
                     std::pow(2.0, frac_bits / 2.0) /
                     std::sqrt(static_cast<double>((1ULL << value_bits) - 1))));
}

auto isqrt_gamut_param(unsigned int value_bits, unsigned int frac_bits,
                       unsigned int output_frac_bits, u64 tolerance)
    -> IsqrtParam {
  return {(1ULL << value_bits) - 1, frac_bits, output_frac_bits, tolerance,
          isqrt_gamut_expected_result(value_bits, frac_bits, output_frac_bits)};
}

#if 1
const IsqrtParam isqrt_smoke_test[] = {
#if 1
    // Identity Case
    // isqrt(1.0) == 1.0.
    // Basic baseline check.
    {1LL << 30, 30, 30, 0, 1LL << 30},

    // The "Problem" Case (High Precision Under-unity result)
    // isqrt(2.0) at Q61. Result is ~0.707.
    // This fails if internal precision doesn't have guard bits for RNE.
    // Error: ~600
    // Expected: round(2^61/sqrt(2))
    {2LL << 61, 61, 61, 524012689, 1630477228166597777ULL},

    // The "Overflow" Risk Case (High Precision Over-unity result)
    // isqrt(0.5) at Q61. Result is sqrt(2) (~1.414).
    // Error: ~200k
    // Expected: round(2^61/sqrt(0.5))
    {1LL << 60, 61, 61, 1048025377, 3260954456333195553ULL},

    // Pure Integer Input
    // isqrt(100) == 0.1
    // Checks standard integer handling and large rescaling (Q0 -> Q60).
    // Expected: round(2^60/sqrt(100))
    {100, 0, 60, 1, 115292150460684698ULL},

    // Irrational Non-Power-of-2
    // isqrt(3.0) at Q60.
    // Checks rounding logic on standard messy numbers.
    // Error: ~1.2k
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
    {6LL << 60, 60, 60, 0, 470678233243713536ULL},

    // The "Precision Cliff" (Alternating Bits)
    // Input: 0x5...5 (0.333...). High entropy bit pattern.
    // The mathematical result lands at ...025.5002.
    // This is the ultimate test of your Fused Update and RNE.
    // If you have ANY bias, you will snap to ...025. If correct, ...026.
    // Expected: round(2^60 / sqrt(0x555... * 2^-60))
    // Error ~1M
    {0x5555555555555555LL, 60, 60, 101919389, 499229655779453597LL},

    // THE FLOOR (Flat Slope)
    // Input: S64_MAX (in Q0). This is the largest possible input.
    // Math: 1 / sqrt(2^63 - 1) approx 1 / 3,037,000,499.
    // Result is very small. This tests if we lose bits when y is tiny.
    // Expected: 2^60 * (1/sqrt(2^63-1))
    // Error ~7M
    {S64_MAX, 0, 60, 0, 379625062LL},

    // THE CEILING (Steep Slope)
    // Input: 1 (in Q0). This is x=1.0.
    // Math: 1 / sqrt(1). Result 1.0.
    // This checks the Identity at maximum integer magnitude.
    {1, 0, 60, 0, 1LL << 60},

    // THE "ALMOST" POWER OF 2 (CLZ Stress)
    // Input: (1<<62) - 1. This is all 1s in the high bits.
    // It has the same CLZ as (1<<62), but a vastly different mantissa.
    // This breaks algorithms that rely too heavily on the exponent guess.
    // Math: 1 / sqrt(4.0 - epsilon) -> Just slightly > 0.5
    // 0.500000000000000000135 * 2^60
    // Error ~59
    {(1LL << 62) - 1, 60, 60, 59, 576460752303423547LL},

    // THE SATURATION RISK (Output Overflow)
    // Input: Smallest normalized x in Q30 (value 1).
    // Math: x = 2^-30. 1/sqrt(x) = 2^15 = 32768.
    // We request Output Q50. Result: 32768 * 2^50 = 2^65.
    // This MUST saturate to U64_MAX.
    {1, 30, 50, 0, U64_MAX},

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
#endif
};
INSTANTIATE_TEST_SUITE_P(smoke_tests, IsqrtTest, ValuesIn(isqrt_smoke_test));
#endif

// clang-format off
const IsqrtParam isqrt_gamut[] = {
    // 1 output bits    
    isqrt_gamut_param( 1,  0,  1, 0),
    isqrt_gamut_param( 2,  0,  1, 0),
    isqrt_gamut_param( 4,  0,  1, 0),
    isqrt_gamut_param( 8,  0,  1, 0),
    isqrt_gamut_param(16,  0,  1, 0),
    isqrt_gamut_param(24,  0,  1, 0),
    isqrt_gamut_param(32,  0,  1, 0),
    isqrt_gamut_param(48,  0,  1, 0),
    isqrt_gamut_param(62,  0,  1, 0),
    isqrt_gamut_param(63,  0,  1, 0),
    isqrt_gamut_param( 1,  1,  1, 0),
    isqrt_gamut_param( 2,  1,  1, 0),
    isqrt_gamut_param( 4,  1,  1, 0),
    isqrt_gamut_param( 8,  1,  1, 0),
    isqrt_gamut_param(16,  1,  1, 0),
    isqrt_gamut_param(24,  1,  1, 0),
    isqrt_gamut_param(32,  1,  1, 0),
    isqrt_gamut_param(48,  1,  1, 0),
    isqrt_gamut_param(62,  1,  1, 0),
    isqrt_gamut_param(63,  1,  1, 0),
    isqrt_gamut_param( 1,  2,  1, 0),
    isqrt_gamut_param( 2,  2,  1, 0),
    isqrt_gamut_param( 4,  2,  1, 0),
    isqrt_gamut_param( 8,  2,  1, 0),
    isqrt_gamut_param(16,  2,  1, 0),
    isqrt_gamut_param(24,  2,  1, 0),
    isqrt_gamut_param(32,  2,  1, 0),
    isqrt_gamut_param(48,  2,  1, 0),
    isqrt_gamut_param(62,  2,  1, 0),
    isqrt_gamut_param(63,  2,  1, 0),
    isqrt_gamut_param( 1,  4,  1, 0),
    isqrt_gamut_param( 2,  4,  1, 0),
    isqrt_gamut_param( 4,  4,  1, 0),
    isqrt_gamut_param( 8,  4,  1, 0),
    isqrt_gamut_param(16,  4,  1, 0),
    isqrt_gamut_param(24,  4,  1, 0),
    isqrt_gamut_param(32,  4,  1, 0),
    isqrt_gamut_param(48,  4,  1, 0),
    isqrt_gamut_param(62,  4,  1, 0),
    isqrt_gamut_param(63,  4,  1, 0),
    isqrt_gamut_param( 1,  8,  1, 0),
    isqrt_gamut_param( 2,  8,  1, 0),
    isqrt_gamut_param( 4,  8,  1, 0),
    isqrt_gamut_param( 8,  8,  1, 0),
    isqrt_gamut_param(16,  8,  1, 0),
    isqrt_gamut_param(24,  8,  1, 0),
    isqrt_gamut_param(32,  8,  1, 0),
    isqrt_gamut_param(48,  8,  1, 0),
    isqrt_gamut_param(62,  8,  1, 0),
    isqrt_gamut_param(63,  8,  1, 0),
    isqrt_gamut_param( 1, 16,  1, 0),
    isqrt_gamut_param( 2, 16,  1, 0),
    isqrt_gamut_param( 4, 16,  1, 0),
    isqrt_gamut_param( 8, 16,  1, 0),
    isqrt_gamut_param(16, 16,  1, 0),
    isqrt_gamut_param(24, 16,  1, 0),
    isqrt_gamut_param(32, 16,  1, 0),
    isqrt_gamut_param(48, 16,  1, 0),
    isqrt_gamut_param(62, 16,  1, 0),
    isqrt_gamut_param(63, 16,  1, 0),
    isqrt_gamut_param( 1, 24,  1, 0),
    isqrt_gamut_param( 2, 24,  1, 0),
    isqrt_gamut_param( 4, 24,  1, 0),
    isqrt_gamut_param( 8, 24,  1, 0),
    isqrt_gamut_param(16, 24,  1, 0),
    isqrt_gamut_param(24, 24,  1, 0),
    isqrt_gamut_param(32, 24,  1, 0),
    isqrt_gamut_param(48, 24,  1, 0),
    isqrt_gamut_param(62, 24,  1, 0),
    isqrt_gamut_param(63, 24,  1, 0),
    isqrt_gamut_param( 1, 32,  1, 0),
    isqrt_gamut_param( 2, 32,  1, 0),
    isqrt_gamut_param( 4, 32,  1, 0),
    isqrt_gamut_param( 8, 32,  1, 0),
    isqrt_gamut_param(16, 32,  1, 0),
    isqrt_gamut_param(24, 32,  1, 0),
    isqrt_gamut_param(32, 32,  1, 0),
    isqrt_gamut_param(48, 32,  1, 0),
    isqrt_gamut_param(62, 32,  1, 0),
    isqrt_gamut_param(63, 32,  1, 0),
    isqrt_gamut_param( 1, 48,  1, 0),
    isqrt_gamut_param( 2, 48,  1, 0),
    isqrt_gamut_param( 4, 48,  1, 0),
    isqrt_gamut_param( 8, 48,  1, 0),
    isqrt_gamut_param(16, 48,  1, 0),
    isqrt_gamut_param(24, 48,  1, 0),
    isqrt_gamut_param(32, 48,  1, 0),
    isqrt_gamut_param(48, 48,  1, 0),
    isqrt_gamut_param(62, 48,  1, 0),
    isqrt_gamut_param(63, 48,  1, 0),
    isqrt_gamut_param( 1, 62,  1, 0),
    isqrt_gamut_param( 2, 62,  1, 0),
    isqrt_gamut_param( 4, 62,  1, 0),
    isqrt_gamut_param( 8, 62,  1, 0),
    isqrt_gamut_param(16, 62,  1, 0),
    isqrt_gamut_param(24, 62,  1, 0),
    isqrt_gamut_param(32, 62,  1, 0),
    isqrt_gamut_param(48, 62,  1, 0),
    isqrt_gamut_param(62, 62,  1, 0),
    isqrt_gamut_param(63, 62,  1, 0),
    isqrt_gamut_param( 1, 63,  1, 0),
    isqrt_gamut_param( 2, 63,  1, 0),
    isqrt_gamut_param( 4, 63,  1, 0),
    isqrt_gamut_param( 8, 63,  1, 0),
    isqrt_gamut_param(16, 63,  1, 0),
    isqrt_gamut_param(24, 63,  1, 0),
    isqrt_gamut_param(32, 63,  1, 0),
    isqrt_gamut_param(48, 63,  1, 0),
    isqrt_gamut_param(62, 63,  1, 0),
    isqrt_gamut_param(63, 63,  1, 0),
    isqrt_gamut_param( 1, 64,  1, 0),
    isqrt_gamut_param( 2, 64,  1, 0),
    isqrt_gamut_param( 4, 64,  1, 0),
    isqrt_gamut_param( 8, 64,  1, 0),
    isqrt_gamut_param(16, 64,  1, 0),
    isqrt_gamut_param(24, 64,  1, 0),
    isqrt_gamut_param(32, 64,  1, 0),
    isqrt_gamut_param(48, 64,  1, 0),
    isqrt_gamut_param(62, 64,  1, 0),
    isqrt_gamut_param(63, 64,  1, 0),
    
    // 2 output bits    
    isqrt_gamut_param( 1,  0,  2, 0),
    isqrt_gamut_param( 2,  0,  2, 0),
    isqrt_gamut_param( 4,  0,  2, 0),
    isqrt_gamut_param( 8,  0,  2, 0),
    isqrt_gamut_param(16,  0,  2, 0),
    isqrt_gamut_param(24,  0,  2, 0),
    isqrt_gamut_param(32,  0,  2, 0),
    isqrt_gamut_param(48,  0,  2, 0),
    isqrt_gamut_param(62,  0,  2, 0),
    isqrt_gamut_param(63,  0,  2, 0),
    isqrt_gamut_param( 1,  1,  2, 0),
    isqrt_gamut_param( 2,  1,  2, 0),
    isqrt_gamut_param( 4,  1,  2, 0),
    isqrt_gamut_param( 8,  1,  2, 0),
    isqrt_gamut_param(16,  1,  2, 0),
    isqrt_gamut_param(24,  1,  2, 0),
    isqrt_gamut_param(32,  1,  2, 0),
    isqrt_gamut_param(48,  1,  2, 0),
    isqrt_gamut_param(62,  1,  2, 0),
    isqrt_gamut_param(63,  1,  2, 0),
    isqrt_gamut_param( 1,  2,  2, 0),
    isqrt_gamut_param( 2,  2,  2, 0),
    isqrt_gamut_param( 4,  2,  2, 0),
    isqrt_gamut_param( 8,  2,  2, 0),
    isqrt_gamut_param(16,  2,  2, 0),
    isqrt_gamut_param(24,  2,  2, 0),
    isqrt_gamut_param(32,  2,  2, 0),
    isqrt_gamut_param(48,  2,  2, 0),
    isqrt_gamut_param(62,  2,  2, 0),
    isqrt_gamut_param(63,  2,  2, 0),
    isqrt_gamut_param( 1,  4,  2, 0),
    isqrt_gamut_param( 2,  4,  2, 0),
    isqrt_gamut_param( 4,  4,  2, 0),
    isqrt_gamut_param( 8,  4,  2, 0),
    isqrt_gamut_param(16,  4,  2, 0),
    isqrt_gamut_param(24,  4,  2, 0),
    isqrt_gamut_param(32,  4,  2, 0),
    isqrt_gamut_param(48,  4,  2, 0),
    isqrt_gamut_param(62,  4,  2, 0),
    isqrt_gamut_param(63,  4,  2, 0),
    isqrt_gamut_param( 1,  8,  2, 0),
    isqrt_gamut_param( 2,  8,  2, 0),
    isqrt_gamut_param( 4,  8,  2, 0),
    isqrt_gamut_param( 8,  8,  2, 0),
    isqrt_gamut_param(16,  8,  2, 0),
    isqrt_gamut_param(24,  8,  2, 0),
    isqrt_gamut_param(32,  8,  2, 0),
    isqrt_gamut_param(48,  8,  2, 0),
    isqrt_gamut_param(62,  8,  2, 0),
    isqrt_gamut_param(63,  8,  2, 0),
    isqrt_gamut_param( 1, 16,  2, 0),
    isqrt_gamut_param( 2, 16,  2, 0),
    isqrt_gamut_param( 4, 16,  2, 0),
    isqrt_gamut_param( 8, 16,  2, 0),
    isqrt_gamut_param(16, 16,  2, 0),
    isqrt_gamut_param(24, 16,  2, 0),
    isqrt_gamut_param(32, 16,  2, 0),
    isqrt_gamut_param(48, 16,  2, 0),
    isqrt_gamut_param(62, 16,  2, 0),
    isqrt_gamut_param(63, 16,  2, 0),
    isqrt_gamut_param( 1, 24,  2, 0),
    isqrt_gamut_param( 2, 24,  2, 0),
    isqrt_gamut_param( 4, 24,  2, 0),
    isqrt_gamut_param( 8, 24,  2, 0),
    isqrt_gamut_param(16, 24,  2, 0),
    isqrt_gamut_param(24, 24,  2, 0),
    isqrt_gamut_param(32, 24,  2, 0),
    isqrt_gamut_param(48, 24,  2, 0),
    isqrt_gamut_param(62, 24,  2, 0),
    isqrt_gamut_param(63, 24,  2, 0),
    isqrt_gamut_param( 1, 32,  2, 0),
    isqrt_gamut_param( 2, 32,  2, 0),
    isqrt_gamut_param( 4, 32,  2, 0),
    isqrt_gamut_param( 8, 32,  2, 0),
    isqrt_gamut_param(16, 32,  2, 0),
    isqrt_gamut_param(24, 32,  2, 0),
    isqrt_gamut_param(32, 32,  2, 0),
    isqrt_gamut_param(48, 32,  2, 0),
    isqrt_gamut_param(62, 32,  2, 0),
    isqrt_gamut_param(63, 32,  2, 0),
    isqrt_gamut_param( 1, 48,  2, 0),
    isqrt_gamut_param( 2, 48,  2, 0),
    isqrt_gamut_param( 4, 48,  2, 0),
    isqrt_gamut_param( 8, 48,  2, 0),
    isqrt_gamut_param(16, 48,  2, 0),
    isqrt_gamut_param(24, 48,  2, 0),
    isqrt_gamut_param(32, 48,  2, 0),
    isqrt_gamut_param(48, 48,  2, 0),
    isqrt_gamut_param(62, 48,  2, 0),
    isqrt_gamut_param(63, 48,  2, 0),
    isqrt_gamut_param( 1, 62,  2, 0),
    isqrt_gamut_param( 2, 62,  2, 0),
    isqrt_gamut_param( 4, 62,  2, 0),
    isqrt_gamut_param( 8, 62,  2, 0),
    isqrt_gamut_param(16, 62,  2, 0),
    isqrt_gamut_param(24, 62,  2, 0),
    isqrt_gamut_param(32, 62,  2, 0),
    isqrt_gamut_param(48, 62,  2, 0),
    isqrt_gamut_param(62, 62,  2, 0),
    isqrt_gamut_param(63, 62,  2, 0),
    isqrt_gamut_param( 1, 63,  2, 0),
    isqrt_gamut_param( 2, 63,  2, 0),
    isqrt_gamut_param( 4, 63,  2, 0),
    isqrt_gamut_param( 8, 63,  2, 0),
    isqrt_gamut_param(16, 63,  2, 0),
    isqrt_gamut_param(24, 63,  2, 0),
    isqrt_gamut_param(32, 63,  2, 0),
    isqrt_gamut_param(48, 63,  2, 0),
    isqrt_gamut_param(62, 63,  2, 0),
    isqrt_gamut_param(63, 63,  2, 0),
    isqrt_gamut_param( 1, 64,  2, 0),
    isqrt_gamut_param( 2, 64,  2, 0),
    isqrt_gamut_param( 4, 64,  2, 0),
    isqrt_gamut_param( 8, 64,  2, 0),
    isqrt_gamut_param(16, 64,  2, 0),
    isqrt_gamut_param(24, 64,  2, 0),
    isqrt_gamut_param(32, 64,  2, 0),
    isqrt_gamut_param(48, 64,  2, 0),
    isqrt_gamut_param(62, 64,  2, 0),
    isqrt_gamut_param(63, 64,  2, 0),
    
    // 4 output bits    
    isqrt_gamut_param( 1,  0,  4, 0),
    isqrt_gamut_param( 2,  0,  4, 0),
    isqrt_gamut_param( 4,  0,  4, 0),
    isqrt_gamut_param( 8,  0,  4, 0),
    isqrt_gamut_param(16,  0,  4, 0),
    isqrt_gamut_param(24,  0,  4, 0),
    isqrt_gamut_param(32,  0,  4, 0),
    isqrt_gamut_param(48,  0,  4, 0),
    isqrt_gamut_param(62,  0,  4, 0),
    isqrt_gamut_param(63,  0,  4, 0),
    isqrt_gamut_param( 1,  1,  4, 0),
    isqrt_gamut_param( 2,  1,  4, 0),
    isqrt_gamut_param( 4,  1,  4, 0),
    isqrt_gamut_param( 8,  1,  4, 0),
    isqrt_gamut_param(16,  1,  4, 0),
    isqrt_gamut_param(24,  1,  4, 0),
    isqrt_gamut_param(32,  1,  4, 0),
    isqrt_gamut_param(48,  1,  4, 0),
    isqrt_gamut_param(62,  1,  4, 0),
    isqrt_gamut_param(63,  1,  4, 0),
    isqrt_gamut_param( 1,  2,  4, 0),
    isqrt_gamut_param( 2,  2,  4, 0),
    isqrt_gamut_param( 4,  2,  4, 0),
    isqrt_gamut_param( 8,  2,  4, 0),
    isqrt_gamut_param(16,  2,  4, 0),
    isqrt_gamut_param(24,  2,  4, 0),
    isqrt_gamut_param(32,  2,  4, 0),
    isqrt_gamut_param(48,  2,  4, 0),
    isqrt_gamut_param(62,  2,  4, 0),
    isqrt_gamut_param(63,  2,  4, 0),
    isqrt_gamut_param( 1,  4,  4, 0),
    isqrt_gamut_param( 2,  4,  4, 0),
    isqrt_gamut_param( 4,  4,  4, 0),
    isqrt_gamut_param( 8,  4,  4, 0),
    isqrt_gamut_param(16,  4,  4, 0),
    isqrt_gamut_param(24,  4,  4, 0),
    isqrt_gamut_param(32,  4,  4, 0),
    isqrt_gamut_param(48,  4,  4, 0),
    isqrt_gamut_param(62,  4,  4, 0),
    isqrt_gamut_param(63,  4,  4, 0),
    isqrt_gamut_param( 1,  8,  4, 0),
    isqrt_gamut_param( 2,  8,  4, 0),
    isqrt_gamut_param( 4,  8,  4, 0),
    isqrt_gamut_param( 8,  8,  4, 0),
    isqrt_gamut_param(16,  8,  4, 0),
    isqrt_gamut_param(24,  8,  4, 0),
    isqrt_gamut_param(32,  8,  4, 0),
    isqrt_gamut_param(48,  8,  4, 0),
    isqrt_gamut_param(62,  8,  4, 0),
    isqrt_gamut_param(63,  8,  4, 0),
    isqrt_gamut_param( 1, 16,  4, 0),
    isqrt_gamut_param( 2, 16,  4, 0),
    isqrt_gamut_param( 4, 16,  4, 0),
    isqrt_gamut_param( 8, 16,  4, 0),
    isqrt_gamut_param(16, 16,  4, 0),
    isqrt_gamut_param(24, 16,  4, 0),
    isqrt_gamut_param(32, 16,  4, 0),
    isqrt_gamut_param(48, 16,  4, 0),
    isqrt_gamut_param(62, 16,  4, 0),
    isqrt_gamut_param(63, 16,  4, 0),
    isqrt_gamut_param( 1, 24,  4, 0),
    isqrt_gamut_param( 2, 24,  4, 0),
    isqrt_gamut_param( 4, 24,  4, 0),
    isqrt_gamut_param( 8, 24,  4, 0),
    isqrt_gamut_param(16, 24,  4, 0),
    isqrt_gamut_param(24, 24,  4, 0),
    isqrt_gamut_param(32, 24,  4, 0),
    isqrt_gamut_param(48, 24,  4, 0),
    isqrt_gamut_param(62, 24,  4, 0),
    isqrt_gamut_param(63, 24,  4, 0),
    isqrt_gamut_param( 1, 32,  4, 0),
    isqrt_gamut_param( 2, 32,  4, 0),
    isqrt_gamut_param( 4, 32,  4, 0),
    isqrt_gamut_param( 8, 32,  4, 0),
    isqrt_gamut_param(16, 32,  4, 0),
    isqrt_gamut_param(24, 32,  4, 0),
    isqrt_gamut_param(32, 32,  4, 0),
    isqrt_gamut_param(48, 32,  4, 0),
    isqrt_gamut_param(62, 32,  4, 0),
    isqrt_gamut_param(63, 32,  4, 0),
    isqrt_gamut_param( 1, 48,  4, 0),
    isqrt_gamut_param( 2, 48,  4, 0),
    isqrt_gamut_param( 4, 48,  4, 0),
    isqrt_gamut_param( 8, 48,  4, 0),
    isqrt_gamut_param(16, 48,  4, 0),
    isqrt_gamut_param(24, 48,  4, 0),
    isqrt_gamut_param(32, 48,  4, 0),
    isqrt_gamut_param(48, 48,  4, 0),
    isqrt_gamut_param(62, 48,  4, 0),
    isqrt_gamut_param(63, 48,  4, 0),
    isqrt_gamut_param( 1, 62,  4, 0),
    isqrt_gamut_param( 2, 62,  4, 0),
    isqrt_gamut_param( 4, 62,  4, 0),
    isqrt_gamut_param( 8, 62,  4, 0),
    isqrt_gamut_param(16, 62,  4, 0),
    isqrt_gamut_param(24, 62,  4, 0),
    isqrt_gamut_param(32, 62,  4, 0),
    isqrt_gamut_param(48, 62,  4, 0),
    isqrt_gamut_param(62, 62,  4, 0),
    isqrt_gamut_param(63, 62,  4, 0),
    isqrt_gamut_param( 1, 63,  4, 0),
    isqrt_gamut_param( 2, 63,  4, 0),
    isqrt_gamut_param( 4, 63,  4, 0),
    isqrt_gamut_param( 8, 63,  4, 0),
    isqrt_gamut_param(16, 63,  4, 0),
    isqrt_gamut_param(24, 63,  4, 0),
    isqrt_gamut_param(32, 63,  4, 0),
    isqrt_gamut_param(48, 63,  4, 0),
    isqrt_gamut_param(62, 63,  4, 0),
    isqrt_gamut_param(63, 63,  4, 0),
    isqrt_gamut_param( 1, 64,  4, 0),
    isqrt_gamut_param( 2, 64,  4, 0),
    isqrt_gamut_param( 4, 64,  4, 0),
    isqrt_gamut_param( 8, 64,  4, 0),
    isqrt_gamut_param(16, 64,  4, 0),
    isqrt_gamut_param(24, 64,  4, 1),
    isqrt_gamut_param(32, 64,  4, 0),
    isqrt_gamut_param(48, 64,  4, 0),
    isqrt_gamut_param(62, 64,  4, 0),
    isqrt_gamut_param(63, 64,  4, 0),
    
    // 8 output bits    
    isqrt_gamut_param( 1,  0,  8, 0),
    isqrt_gamut_param( 2,  0,  8, 0),
    isqrt_gamut_param( 4,  0,  8, 0),
    isqrt_gamut_param( 8,  0,  8, 0),
    isqrt_gamut_param(16,  0,  8, 0),
    isqrt_gamut_param(24,  0,  8, 0),
    isqrt_gamut_param(32,  0,  8, 0),
    isqrt_gamut_param(48,  0,  8, 0),
    isqrt_gamut_param(62,  0,  8, 0),
    isqrt_gamut_param(63,  0,  8, 0),
    isqrt_gamut_param( 1,  1,  8, 0),
    isqrt_gamut_param( 2,  1,  8, 0),
    isqrt_gamut_param( 4,  1,  8, 0),
    isqrt_gamut_param( 8,  1,  8, 0),
    isqrt_gamut_param(16,  1,  8, 0),
    isqrt_gamut_param(24,  1,  8, 0),
    isqrt_gamut_param(32,  1,  8, 0),
    isqrt_gamut_param(48,  1,  8, 0),
    isqrt_gamut_param(62,  1,  8, 0),
    isqrt_gamut_param(63,  1,  8, 0),
    isqrt_gamut_param( 1,  2,  8, 0),
    isqrt_gamut_param( 2,  2,  8, 0),
    isqrt_gamut_param( 4,  2,  8, 0),
    isqrt_gamut_param( 8,  2,  8, 0),
    isqrt_gamut_param(16,  2,  8, 0),
    isqrt_gamut_param(24,  2,  8, 0),
    isqrt_gamut_param(32,  2,  8, 0),
    isqrt_gamut_param(48,  2,  8, 0),
    isqrt_gamut_param(62,  2,  8, 0),
    isqrt_gamut_param(63,  2,  8, 0),
    isqrt_gamut_param( 1,  4,  8, 0),
    isqrt_gamut_param( 2,  4,  8, 0),
    isqrt_gamut_param( 4,  4,  8, 0),
    isqrt_gamut_param( 8,  4,  8, 0),
    isqrt_gamut_param(16,  4,  8, 0),
    isqrt_gamut_param(24,  4,  8, 0),
    isqrt_gamut_param(32,  4,  8, 0),
    isqrt_gamut_param(48,  4,  8, 0),
    isqrt_gamut_param(62,  4,  8, 0),
    isqrt_gamut_param(63,  4,  8, 0),
    isqrt_gamut_param( 1,  8,  8, 0),
    isqrt_gamut_param( 2,  8,  8, 0),
    isqrt_gamut_param( 4,  8,  8, 0),
    isqrt_gamut_param( 8,  8,  8, 0),
    isqrt_gamut_param(16,  8,  8, 0),
    isqrt_gamut_param(24,  8,  8, 0),
    isqrt_gamut_param(32,  8,  8, 0),
    isqrt_gamut_param(48,  8,  8, 0),
    isqrt_gamut_param(62,  8,  8, 0),
    isqrt_gamut_param(63,  8,  8, 0),
    isqrt_gamut_param( 1, 16,  8, 0),
    isqrt_gamut_param( 2, 16,  8, 0),
    isqrt_gamut_param( 4, 16,  8, 0),
    isqrt_gamut_param( 8, 16,  8, 0),
    isqrt_gamut_param(16, 16,  8, 0),
    isqrt_gamut_param(24, 16,  8, 0),
    isqrt_gamut_param(32, 16,  8, 0),
    isqrt_gamut_param(48, 16,  8, 0),
    isqrt_gamut_param(62, 16,  8, 0),
    isqrt_gamut_param(63, 16,  8, 0),
    isqrt_gamut_param( 1, 24,  8, 0),
    isqrt_gamut_param( 2, 24,  8, 0),
    isqrt_gamut_param( 4, 24,  8, 0),
    isqrt_gamut_param( 8, 24,  8, 0),
    isqrt_gamut_param(16, 24,  8, 0),
    isqrt_gamut_param(24, 24,  8, 0),
    isqrt_gamut_param(32, 24,  8, 0),
    isqrt_gamut_param(48, 24,  8, 0),
    isqrt_gamut_param(62, 24,  8, 0),
    isqrt_gamut_param(63, 24,  8, 0),
    isqrt_gamut_param( 1, 32,  8, 0),
    isqrt_gamut_param( 2, 32,  8, 0),
    isqrt_gamut_param( 4, 32,  8, 0),
    isqrt_gamut_param( 8, 32,  8, 0),
    isqrt_gamut_param(16, 32,  8, 0),
    isqrt_gamut_param(24, 32,  8, 0),
    isqrt_gamut_param(32, 32,  8, 0),
    isqrt_gamut_param(48, 32,  8, 0),
    isqrt_gamut_param(62, 32,  8, 0),
    isqrt_gamut_param(63, 32,  8, 0),
    isqrt_gamut_param( 1, 48,  8, 0),
    isqrt_gamut_param( 2, 48,  8, 0),
    isqrt_gamut_param( 4, 48,  8, 0),
    isqrt_gamut_param( 8, 48,  8, 0),
    isqrt_gamut_param(16, 48,  8, 0),
    isqrt_gamut_param(24, 48,  8, 0),
    isqrt_gamut_param(32, 48,  8, 0),
    isqrt_gamut_param(48, 48,  8, 0),
    isqrt_gamut_param(62, 48,  8, 0),
    isqrt_gamut_param(63, 48,  8, 0),
    isqrt_gamut_param( 1, 62,  8, 0),
    isqrt_gamut_param( 2, 62,  8, 0),
    isqrt_gamut_param( 4, 62,  8, 0),
    isqrt_gamut_param( 8, 62,  8, 0),
    isqrt_gamut_param(16, 62,  8, 0),
    isqrt_gamut_param(24, 62,  8, 0),
    isqrt_gamut_param(32, 62,  8, 0),
    isqrt_gamut_param(48, 62,  8, 0),
    isqrt_gamut_param(62, 62,  8, 0),
    isqrt_gamut_param(63, 62,  8, 0),
    isqrt_gamut_param( 1, 63,  8, 0),
    isqrt_gamut_param( 2, 63,  8, 0),
    isqrt_gamut_param( 4, 63,  8, 0),
    isqrt_gamut_param( 8, 63,  8, 0),
    isqrt_gamut_param(16, 63,  8, 0),
    isqrt_gamut_param(24, 63,  8, 0),
    isqrt_gamut_param(32, 63,  8, 0),
    isqrt_gamut_param(48, 63,  8, 0),
    isqrt_gamut_param(62, 63,  8, 0),
    isqrt_gamut_param(63, 63,  8, 0),
    isqrt_gamut_param( 1, 64,  8, 0),
    isqrt_gamut_param( 2, 64,  8, 0),
    isqrt_gamut_param( 4, 64,  8, 0),
    isqrt_gamut_param( 8, 64,  8, 0),
    isqrt_gamut_param(16, 64,  8, 0),
    isqrt_gamut_param(24, 64,  8, 0),
    isqrt_gamut_param(32, 64,  8, 0),
    isqrt_gamut_param(48, 64,  8, 0),
    isqrt_gamut_param(62, 64,  8, 0),
    isqrt_gamut_param(63, 64,  8, 0),
    
    // 16 output bits    
    isqrt_gamut_param( 1,  0, 16, 0),
    isqrt_gamut_param( 2,  0, 16, 0),
    isqrt_gamut_param( 4,  0, 16, 0),
    isqrt_gamut_param( 8,  0, 16, 0),
    isqrt_gamut_param(16,  0, 16, 0),
    isqrt_gamut_param(24,  0, 16, 0),
    isqrt_gamut_param(32,  0, 16, 0),
    isqrt_gamut_param(48,  0, 16, 0),
    isqrt_gamut_param(62,  0, 16, 0),
    isqrt_gamut_param(63,  0, 16, 0),
    isqrt_gamut_param( 1,  1, 16, 0),
    isqrt_gamut_param( 2,  1, 16, 0),
    isqrt_gamut_param( 4,  1, 16, 0),
    isqrt_gamut_param( 8,  1, 16, 0),
    isqrt_gamut_param(16,  1, 16, 0),
    isqrt_gamut_param(24,  1, 16, 0),
    isqrt_gamut_param(32,  1, 16, 0),
    isqrt_gamut_param(48,  1, 16, 0),
    isqrt_gamut_param(62,  1, 16, 0),
    isqrt_gamut_param(63,  1, 16, 0),
    isqrt_gamut_param( 1,  2, 16, 0),
    isqrt_gamut_param( 2,  2, 16, 0),
    isqrt_gamut_param( 4,  2, 16, 0),
    isqrt_gamut_param( 8,  2, 16, 0),
    isqrt_gamut_param(16,  2, 16, 0),
    isqrt_gamut_param(24,  2, 16, 0),
    isqrt_gamut_param(32,  2, 16, 0),
    isqrt_gamut_param(48,  2, 16, 0),
    isqrt_gamut_param(62,  2, 16, 0),
    isqrt_gamut_param(63,  2, 16, 0),
    isqrt_gamut_param( 1,  4, 16, 0),
    isqrt_gamut_param( 2,  4, 16, 0),
    isqrt_gamut_param( 4,  4, 16, 0),
    isqrt_gamut_param( 8,  4, 16, 0),
    isqrt_gamut_param(16,  4, 16, 0),
    isqrt_gamut_param(24,  4, 16, 0),
    isqrt_gamut_param(32,  4, 16, 0),
    isqrt_gamut_param(48,  4, 16, 0),
    isqrt_gamut_param(62,  4, 16, 0),
    isqrt_gamut_param(63,  4, 16, 0),
    isqrt_gamut_param( 1,  8, 16, 0),
    isqrt_gamut_param( 2,  8, 16, 0),
    isqrt_gamut_param( 4,  8, 16, 0),
    isqrt_gamut_param( 8,  8, 16, 0),
    isqrt_gamut_param(16,  8, 16, 0),
    isqrt_gamut_param(24,  8, 16, 0),
    isqrt_gamut_param(32,  8, 16, 0),
    isqrt_gamut_param(48,  8, 16, 0),
    isqrt_gamut_param(62,  8, 16, 0),
    isqrt_gamut_param(63,  8, 16, 0),
    isqrt_gamut_param( 1, 16, 16, 0),
    isqrt_gamut_param( 2, 16, 16, 0),
    isqrt_gamut_param( 4, 16, 16, 0),
    isqrt_gamut_param( 8, 16, 16, 0),
    isqrt_gamut_param(16, 16, 16, 0),
    isqrt_gamut_param(24, 16, 16, 0),
    isqrt_gamut_param(32, 16, 16, 0),
    isqrt_gamut_param(48, 16, 16, 0),
    isqrt_gamut_param(62, 16, 16, 0),
    isqrt_gamut_param(63, 16, 16, 0),
    isqrt_gamut_param( 1, 24, 16, 0),
    isqrt_gamut_param( 2, 24, 16, 0),
    isqrt_gamut_param( 4, 24, 16, 0),
    isqrt_gamut_param( 8, 24, 16, 0),
    isqrt_gamut_param(16, 24, 16, 0),
    isqrt_gamut_param(24, 24, 16, 0),
    isqrt_gamut_param(32, 24, 16, 0),
    isqrt_gamut_param(48, 24, 16, 0),
    isqrt_gamut_param(62, 24, 16, 0),
    isqrt_gamut_param(63, 24, 16, 0),
    isqrt_gamut_param( 1, 32, 16, 0),
    isqrt_gamut_param( 2, 32, 16, 0),
    isqrt_gamut_param( 4, 32, 16, 0),
    isqrt_gamut_param( 8, 32, 16, 0),
    isqrt_gamut_param(16, 32, 16, 0),
    isqrt_gamut_param(24, 32, 16, 0),
    isqrt_gamut_param(32, 32, 16, 0),
    isqrt_gamut_param(48, 32, 16, 0),
    isqrt_gamut_param(62, 32, 16, 0),
    isqrt_gamut_param(63, 32, 16, 0),
    isqrt_gamut_param( 1, 48, 16, 0),
    isqrt_gamut_param( 2, 48, 16, 0),
    isqrt_gamut_param( 4, 48, 16, 0),
    isqrt_gamut_param( 8, 48, 16, 0),
    isqrt_gamut_param(16, 48, 16, 0),
    isqrt_gamut_param(24, 48, 16, 0),
    isqrt_gamut_param(32, 48, 16, 0),
    isqrt_gamut_param(48, 48, 16, 0),
    isqrt_gamut_param(62, 48, 16, 0),
    isqrt_gamut_param(63, 48, 16, 0),
    isqrt_gamut_param( 1, 62, 16, 0),
    isqrt_gamut_param( 2, 62, 16, 0),
    isqrt_gamut_param( 4, 62, 16, 0),
    isqrt_gamut_param( 8, 62, 16, 0),
    isqrt_gamut_param(16, 62, 16, 0),
    isqrt_gamut_param(24, 62, 16, 0),
    isqrt_gamut_param(32, 62, 16, 0),
    isqrt_gamut_param(48, 62, 16, 0),
    isqrt_gamut_param(62, 62, 16, 0),
    isqrt_gamut_param(63, 62, 16, 0),
    isqrt_gamut_param( 1, 63, 16, 0),
    isqrt_gamut_param( 2, 63, 16, 0),
    isqrt_gamut_param( 4, 63, 16, 0),
    isqrt_gamut_param( 8, 63, 16, 0),
    isqrt_gamut_param(16, 63, 16, 0),
    isqrt_gamut_param(24, 63, 16, 0),
    isqrt_gamut_param(32, 63, 16, 0),
    isqrt_gamut_param(48, 63, 16, 0),
    isqrt_gamut_param(62, 63, 16, 0),
    isqrt_gamut_param(63, 63, 16, 0),
    isqrt_gamut_param( 1, 64, 16, 0),
    isqrt_gamut_param( 2, 64, 16, 0),
    isqrt_gamut_param( 4, 64, 16, 0),
    isqrt_gamut_param( 8, 64, 16, 0),
    isqrt_gamut_param(16, 64, 16, 0),
    isqrt_gamut_param(24, 64, 16, 0),
    isqrt_gamut_param(32, 64, 16, 0),
    isqrt_gamut_param(48, 64, 16, 0),
    isqrt_gamut_param(62, 64, 16, 0),
    isqrt_gamut_param(63, 64, 16, 0),
    
    // 24 output bits
    isqrt_gamut_param( 1,  0, 24, 0),
    isqrt_gamut_param( 2,  0, 24, 0),
    isqrt_gamut_param( 4,  0, 24, 0),
    isqrt_gamut_param( 8,  0, 24, 0),
    isqrt_gamut_param(16,  0, 24, 0),
    isqrt_gamut_param(24,  0, 24, 0),
    isqrt_gamut_param(32,  0, 24, 0),
    isqrt_gamut_param(48,  0, 24, 0),
    isqrt_gamut_param(62,  0, 24, 0),
    isqrt_gamut_param(63,  0, 24, 0),
    isqrt_gamut_param( 1,  1, 24, 0),
    isqrt_gamut_param( 2,  1, 24, 0),
    isqrt_gamut_param( 4,  1, 24, 0),
    isqrt_gamut_param( 8,  1, 24, 0),
    isqrt_gamut_param(16,  1, 24, 0),
    isqrt_gamut_param(24,  1, 24, 0),
    isqrt_gamut_param(32,  1, 24, 0),
    isqrt_gamut_param(48,  1, 24, 0),
    isqrt_gamut_param(62,  1, 24, 0),
    isqrt_gamut_param(63,  1, 24, 0),
    isqrt_gamut_param( 1,  2, 24, 0),
    isqrt_gamut_param( 2,  2, 24, 0),
    isqrt_gamut_param( 4,  2, 24, 0),
    isqrt_gamut_param( 8,  2, 24, 0),
    isqrt_gamut_param(16,  2, 24, 0),
    isqrt_gamut_param(24,  2, 24, 0),
    isqrt_gamut_param(32,  2, 24, 0),
    isqrt_gamut_param(48,  2, 24, 0),
    isqrt_gamut_param(62,  2, 24, 0),
    isqrt_gamut_param(63,  2, 24, 0),
    isqrt_gamut_param( 1,  4, 24, 0),
    isqrt_gamut_param( 2,  4, 24, 0),
    isqrt_gamut_param( 4,  4, 24, 0),
    isqrt_gamut_param( 8,  4, 24, 0),
    isqrt_gamut_param(16,  4, 24, 0),
    isqrt_gamut_param(24,  4, 24, 0),
    isqrt_gamut_param(32,  4, 24, 0),
    isqrt_gamut_param(48,  4, 24, 0),
    isqrt_gamut_param(62,  4, 24, 0),
    isqrt_gamut_param(63,  4, 24, 0),
    isqrt_gamut_param( 1,  8, 24, 0),
    isqrt_gamut_param( 2,  8, 24, 0),
    isqrt_gamut_param( 4,  8, 24, 0),
    isqrt_gamut_param( 8,  8, 24, 0),
    isqrt_gamut_param(16,  8, 24, 0),
    isqrt_gamut_param(24,  8, 24, 0),
    isqrt_gamut_param(32,  8, 24, 0),
    isqrt_gamut_param(48,  8, 24, 0),
    isqrt_gamut_param(62,  8, 24, 0),
    isqrt_gamut_param(63,  8, 24, 0),
    isqrt_gamut_param( 1, 16, 24, 0),
    isqrt_gamut_param( 2, 16, 24, 0),
    isqrt_gamut_param( 4, 16, 24, 0),
    isqrt_gamut_param( 8, 16, 24, 0),
    isqrt_gamut_param(16, 16, 24, 0),
    isqrt_gamut_param(24, 16, 24, 0),
    isqrt_gamut_param(32, 16, 24, 0),
    isqrt_gamut_param(48, 16, 24, 0),
    isqrt_gamut_param(62, 16, 24, 0),
    isqrt_gamut_param(63, 16, 24, 0),
    isqrt_gamut_param( 1, 24, 24, 0),
    isqrt_gamut_param( 2, 24, 24, 0),
    isqrt_gamut_param( 4, 24, 24, 0),
    isqrt_gamut_param( 8, 24, 24, 0),
    isqrt_gamut_param(16, 24, 24, 0),
    isqrt_gamut_param(24, 24, 24, 1),
    isqrt_gamut_param(32, 24, 24, 0),
    isqrt_gamut_param(48, 24, 24, 0),
    isqrt_gamut_param(62, 24, 24, 0),
    isqrt_gamut_param(63, 24, 24, 0),
    isqrt_gamut_param( 1, 32, 24, 0),
    isqrt_gamut_param( 2, 32, 24, 0),
    isqrt_gamut_param( 4, 32, 24, 0),
    isqrt_gamut_param( 8, 32, 24, 0),
    isqrt_gamut_param(16, 32, 24, 0),
    isqrt_gamut_param(24, 32, 24, 0),
    isqrt_gamut_param(32, 32, 24, 0),
    isqrt_gamut_param(48, 32, 24, 0),
    isqrt_gamut_param(62, 32, 24, 0),
    isqrt_gamut_param(63, 32, 24, 0),
    isqrt_gamut_param( 1, 48, 24, 0),
    isqrt_gamut_param( 2, 48, 24, 0),
    isqrt_gamut_param( 4, 48, 24, 0),
    isqrt_gamut_param( 8, 48, 24, 0),
    isqrt_gamut_param(16, 48, 24, 0),
    isqrt_gamut_param(24, 48, 24, 0),
    isqrt_gamut_param(32, 48, 24, 0),
    isqrt_gamut_param(48, 48, 24, 0),
    isqrt_gamut_param(62, 48, 24, 0),
    isqrt_gamut_param(63, 48, 24, 0),
    isqrt_gamut_param( 1, 62, 24, 0),
    isqrt_gamut_param( 2, 62, 24, 3),
    isqrt_gamut_param( 4, 62, 24, 1),
    isqrt_gamut_param( 8, 62, 24, 0),
    isqrt_gamut_param(16, 62, 24, 0),
    isqrt_gamut_param(24, 62, 24, 0),
    isqrt_gamut_param(32, 62, 24, 64),
    isqrt_gamut_param(48, 62, 24, 0),
    isqrt_gamut_param(62, 62, 24, 0),
    isqrt_gamut_param(63, 62, 24, 0),

    isqrt_gamut_param( 1, 63, 24, 3),
    isqrt_gamut_param( 2, 63, 24, 4),
    isqrt_gamut_param( 4, 63, 24, 1),
    isqrt_gamut_param( 8, 63, 24, 0),
    isqrt_gamut_param(16, 63, 24, 0),
    isqrt_gamut_param(24, 63, 24, 6),
    isqrt_gamut_param(32, 63, 24, 84),
    isqrt_gamut_param(48, 63, 24, 0),
    isqrt_gamut_param(62, 63, 24, 0),
    isqrt_gamut_param(63, 63, 24, 0),
    isqrt_gamut_param( 1, 64, 24, 0),
    isqrt_gamut_param( 2, 64, 24, 6),
    isqrt_gamut_param( 4, 64, 24, 2),
    isqrt_gamut_param( 8, 64, 24, 0),
    isqrt_gamut_param(16, 64, 24, 0),
    isqrt_gamut_param(24, 64, 24, 0),
    isqrt_gamut_param(32, 64, 24, 128),
    isqrt_gamut_param(48, 64, 24, 0),
    isqrt_gamut_param(62, 64, 24, 0),
    isqrt_gamut_param(63, 64, 24, 0),
};
INSTANTIATE_TEST_SUITE_P(gamut, IsqrtTest, ValuesIn(isqrt_gamut));
// clang-format on
#endif

}  // namespace
}  // namespace curves
