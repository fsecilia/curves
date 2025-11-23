// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <curves/test.hpp>
#include <curves/fixed.hpp>

namespace curves {
namespace {

// ----------------------------------------------------------------------------
// __curves_fixed_divide_error()
// ----------------------------------------------------------------------------

struct DivideErrorTestParam {
  s64 dividend;
  s64 divisor;
  s64 expected_result;

  friend auto operator<<(std::ostream& out, const DivideErrorTestParam& src)
      -> std::ostream& {
    return out << "{" << src.dividend << ", " << src.divisor << ", "
               << src.expected_result << "}";
  }
};

struct FixedDivideErrorTest : TestWithParam<DivideErrorTestParam> {};

TEST_P(FixedDivideErrorTest, expected_result) {
  const auto actual_result =
      __curves_fixed_divide_error(GetParam().dividend, GetParam().divisor);

  const auto expected_result = GetParam().expected_result;

  ASSERT_EQ(expected_result, actual_result);
}

// Zero dividend always returns 0 regardless of divisor or shift.
const DivideErrorTestParam divide_error_zero_dividend[] = {
    {0, 0, 0},        // All parameters zero
    {0, 1, 0},        // Non-zero divisor
    {0, -1, 0},       // Negative divisor
    {0, S64_MIN, 0},  // Minimum divisor
    {0, S64_MAX, 0},  // Maximum divisor
};
INSTANTIATE_TEST_SUITE_P(zero_dividend, FixedDivideErrorTest,
                         ValuesIn(divide_error_zero_dividend));

// Division by zero saturates based on dividend sign.
const DivideErrorTestParam divide_error_division_by_zero[] = {
    // Negative dividends saturate to S64_MIN
    {-1, 0, S64_MIN},
    {-100, 0, S64_MIN},
    {S64_MIN, 0, S64_MIN},

    // Positive dividends saturate to S64_MAX
    {1, 0, S64_MAX},
    {100, 0, S64_MAX},
    {S64_MAX, 0, S64_MAX},
};
INSTANTIATE_TEST_SUITE_P(division_by_zero, FixedDivideErrorTest,
                         ValuesIn(divide_error_division_by_zero));

// Invalid parameters cause saturation based on quotient sign.
const DivideErrorTestParam divide_error_saturation[] = {
    // Negative quotient (different signs) -> S64_MIN
    {1, -1, S64_MIN},
    {-1, 1, S64_MIN},
    {100, -50, S64_MIN},
    {-100, 50, S64_MIN},
    {S64_MIN, 1, S64_MIN},
    {S64_MAX, -1, S64_MIN},

    // Positive quotient (same signs) -> S64_MAX
    {1, 1, S64_MAX},
    {-1, -1, S64_MAX},
    {100, 50, S64_MAX},
    {-100, -50, S64_MAX},
    {S64_MIN, -1, S64_MAX},
    {S64_MAX, 1, S64_MAX},
};
INSTANTIATE_TEST_SUITE_P(saturation, FixedDivideErrorTest,
                         ValuesIn(divide_error_saturation));

// ----------------------------------------------------------------------------
// __curves_fixed_divide_optimal_shift()
// ----------------------------------------------------------------------------

struct DivideOptimalShiftParams {
  u64 dividend;
  u64 divisor;
  int expected_shift;

  friend auto operator<<(std::ostream& out, const DivideOptimalShiftParams& src)
      -> std::ostream& {
    return out << "{" << src.dividend << ", " << src.divisor << ", "
               << src.expected_shift << " } ";
  }
};

struct DivideOptimalShiftTest : TestWithParam<DivideOptimalShiftParams> {};

TEST_P(DivideOptimalShiftTest, expected_result) {
  const auto actual_shift = __curves_fixed_divide_optimal_shift(
      GetParam().dividend, GetParam().divisor);

  ASSERT_EQ(GetParam().expected_shift, actual_shift);
}

/*
  Identity and Basics

  Baseline sanity checks.
*/
const DivideOptimalShiftParams divide_optimal_shift_basics[] = {
    // 1 / 1 -> Shift 63.
    // Dividend is not smaller, so we shift conservatively.
    {1, 1, 63},

    // 1 / 2 -> Shift 64.
    // Divisor is larger, so we can shift dividend by one more bit.
    {1, 2, 64},

    // 2 / 1 -> Shift 62.
    // Dividend is larger, so shift is conservative.
    {2, 1, 62},

    // 100 / 10 -> Shift 60.
    // 64 + clz(100) - clz(10) - 1 -> 64 + 57 - 60 - 1 = 60.
    // Dividend is larger, so shift is conservative.
    {100, 10, 60},
};
INSTANTIATE_TEST_SUITE_P(basics, DivideOptimalShiftTest,
                         ValuesIn(divide_optimal_shift_basics));

/*
  Zero Dividend (The | 1 Trick)

  Verifies the branchless behavior when we use the clz trick to avoid checking
  (dividend == 0) explicitly.
*/
const DivideOptimalShiftParams divide_optimal_shift_zeros[] = {
    // 0 / 1.
    // Internal logic: clz(0 | 1) -> clz(1) -> 63.
    // Result: 64 + 63 - 63 = 64.
    {0, 1, 64},

    // 0 / S64_MAX.
    // clz(dividend) = 63. clz(divisor) = 1.
    // 64 + 63 - 1 - 1 = 125.
    {0, S64_MAX, 125},

    // 0 / U64_MAX.
    // clz(dividend) = 63. clz(divisor) = 0.
    // 64 + 63 - 0 - 1 = 126.
    {0, U64_MAX, 126},
};
INSTANTIATE_TEST_SUITE_P(zeros, DivideOptimalShiftTest,
                         ValuesIn(divide_optimal_shift_zeros));

// This test was relevant when division was based on s64. Now that it is based
// on u64, I'm not sure there is an analogue.
#if 0
/*
  Sign Invariance

  Verifies abs() is working. The shift should only depend on magnitude.
*/
const DivideOptimalShiftParams divide_optimal_shift_signs[] = {
    // 1 / -1 -> Same as 1 / 1 -> 62
    {1, -1, 62},

    // -1 / 1 -> Same as 1 / 1 -> 62
    {-1, 1, 62},

    // -1 / -1 -> Same as 1 / 1 -> 62
    {-1, -1, 62},
};
INSTANTIATE_TEST_SUITE_P(Signs, DivideOptimalShiftTest,
                         ValuesIn(divide_optimal_shift_signs));
#endif

/*
  Extremes and Overflows

  Testing the boundaries of s64.
*/
const DivideOptimalShiftParams divide_optimal_shift_extremes[] = {
    // U64_MAX / 1
    // clz(U64_MAX) = 0. clz(1) = 63.
    // 64 + 0 - 63 - 1 = 0.
    {U64_MAX, 1, 0},

    // S64_MAX / 1
    // clz(S64_MAX) = 1. clz(1) = 63.
    // 64 + 1 - 63 - 1  = 1.
    {S64_MAX, 1, 1},

    // 1 / S64_MAX
    // clz(1) = 63. clz(S64_MAX) = 1.
    // 64 + 63 - 1 - 1 + 1  = 126.
    {1, S64_MAX, 126},

    // S64_MAX / S64_MAX
    // 64 + 1 - 1 - 1 = 63.
    {S64_MAX, S64_MAX, 63},

    // U64_MAX / U64_MAX
    // 64 + 0 - 0 - 1  = 63.
    {U64_MAX, U64_MAX, 63},
};
INSTANTIATE_TEST_SUITE_P(Extremes, DivideOptimalShiftTest,
                         ValuesIn(divide_optimal_shift_extremes));

#if 0
// ----------------------------------------------------------------------------
// curves_fixed_divide()
// ----------------------------------------------------------------------------

struct DivideTestParams {
  s64 dividend;
  unsigned int dividend_frac_bits;
  s64 divisor;
  unsigned int divisor_frac_bits;
  unsigned int output_frac_bits;
  s64 expected_result;

  friend auto operator<<(std::ostream& out, const DivideTestParams& src)
      -> std::ostream& {
    return out << "{" << src.dividend << ", " << src.dividend_frac_bits << ", "
               << src.divisor << ", " << src.divisor_frac_bits << ", "
               << src.output_frac_bits << ", " << src.expected_result << "}";
  }
};

struct FixedDivideTest : TestWithParam<DivideTestParams> {};

TEST_P(FixedDivideTest, expected_result) {
  const auto expected_result = GetParam().expected_result;

  const auto actual_result = curves_fixed_divide(
      GetParam().dividend, GetParam().dividend_frac_bits, GetParam().divisor,
      GetParam().divisor_frac_bits, GetParam().output_frac_bits);

  ASSERT_EQ(expected_result, actual_result);
}

/*
  Invalid Fractional Bits

  Tests that frac_bits >= 64 triggers the error handler correctly.
*/
const DivideTestParams divide_invalid_frac_bits[] = {
    // Invalid dividend_frac_bits
    {-100, 64, 2, 0, 0, S64_MIN},
    {100, 64, 2, 0, 0, S64_MAX},
    {100, 65, 2, 0, 0, S64_MAX},

    // Invalid divisor_frac_bits
    {100, 0, -2, 64, 0, S64_MIN},
    {100, 0, 2, 64, 0, S64_MAX},
    {100, 0, 2, 65, 0, S64_MAX},

    // Invalid output_frac_bits
    {-100, 0, 2, 0, 64, S64_MIN},
    {100, 0, 2, 0, 64, S64_MAX},
    {100, 0, 2, 0, 65, S64_MAX},

    // Multiple invalid parameters
    {100, 64, -2, 64, 64, S64_MIN},
    {100, 64, 2, 64, 64, S64_MAX},
};
INSTANTIATE_TEST_SUITE_P(invalid_frac_bits, FixedDivideTest,
                         ValuesIn(divide_invalid_frac_bits));

/*
  Zero Dividend

  Zero divided by anything (except 0) should always yield 0, regardless of
  precision settings. The optimal shift logic handles 0 with the (| 1) trick.
*/
const DivideTestParams divide_zero_dividend[] = {
    // Zero precision
    {0, 0, 1, 0, 0, 0},
    {0, 0, 100, 0, 0, 0},
    {0, 0, S64_MAX, 0, 0, 0},

    // Mid precision
    {0, 32, 1, 0, 0, 0},
    {0, 32, 1, 32, 32, 0},
    {0, 0, 1, 32, 32, 0},

    // High precision
    {0, 62, S64_MAX, 62, 62, 0},
    {0, 0, 1, 0, 62, 0},

    // Mixed precisions
    {0, 16, 100, 48, 32, 0},
    {0, 48, 1000, 16, 32, 0},

    // Negative divisors
    {0, 0, -1, 0, 0, 0},
    {0, 32, -100, 32, 32, 0},
    {0, 0, S64_MIN, 0, 0, 0},
};
INSTANTIATE_TEST_SUITE_P(zero_dividend, FixedDivideTest,
                         ValuesIn(divide_zero_dividend));

/*
  Division by Zero

  Should saturate based on dividend sign. Positive dividends -> S64_MAX,
  negative dividends -> S64_MIN.
*/
const DivideTestParams divide_by_zero[] = {
    // Positive dividends
    {1, 0, 0, 0, 0, S64_MAX},
    {100, 0, 0, 0, 0, S64_MAX},
    {S64_MAX, 0, 0, 0, 0, S64_MAX},
    {1, 32, 0, 32, 32, S64_MAX},
    {1, 62, 0, 62, 62, S64_MAX},

    // Negative dividends
    {-1, 0, 0, 0, 0, S64_MIN},
    {-100, 0, 0, 0, 0, S64_MIN},
    {S64_MIN, 0, 0, 0, 0, S64_MIN},
    {-1, 32, 0, 32, 32, S64_MIN},
    {-1, 62, 0, 62, 62, S64_MIN},
};
INSTANTIATE_TEST_SUITE_P(division_by_zero, FixedDivideTest,
                         ValuesIn(divide_by_zero));

/*
  Division by One

  Dividing by 1 should preserve the value after rescaling to output precision.
*/
const DivideTestParams divide_by_one[] = {
    // Zero precision - direct pass-through
    {50, 0, 1, 0, 0, 50},
    {-50, 0, 1, 0, 0, -50},
    {S64_MAX, 0, 1, 0, 0, S64_MAX},
    {S64_MIN, 0, 1, 0, 0, S64_MIN},

    // Same input and output precision
    {50LL << 32, 32, 1LL << 32, 32, 32, 50LL << 32},
    {-50LL << 32, 32, 1LL << 32, 32, 32, -50LL << 32},

    // Up-scaling precision
    {50, 0, 1, 0, 32, 50LL << 32},
    {50, 16, 1, 16, 32, 50LL << 32},

    // Down-scaling precision
    {50LL << 32, 32, 1LL << 32, 32, 0, 50},
    {50LL << 48, 48, 1LL << 48, 48, 32, 50LL << 32},
};
INSTANTIATE_TEST_SUITE_P(identity, FixedDivideTest, ValuesIn(divide_by_one));

/*
  Simple Integer Cases

  Basic division with frac_bits = 0 for all parameters.
*/
const DivideTestParams divide_integers[] = {
    // Exact divisions
    {100, 0, 2, 0, 0, 50},
    {1000, 0, 10, 0, 0, 100},
    {144, 0, 12, 0, 0, 12},

    // Truncating divisions (positive)
    {100, 0, 3, 0, 0, 33},
    {100, 0, 7, 0, 0, 14},
    {1000, 0, 3, 0, 0, 333},

    // Truncating divisions (negative dividend)
    {-100, 0, 3, 0, 0, -33},
    {-100, 0, 7, 0, 0, -14},
    {-1000, 0, 3, 0, 0, -333},

    // Truncating divisions (negative divisor)
    {100, 0, -3, 0, 0, -33},
    {100, 0, -7, 0, 0, -14},
    {1000, 0, -3, 0, 0, -333},

    // Truncating divisions (both negative)
    {-100, 0, -3, 0, 0, 33},
    {-100, 0, -7, 0, 0, 14},
    {-1000, 0, -3, 0, 0, 333},

    // Small divisors
    {1000000, 0, 1, 0, 0, 1000000},
    {1000000, 0, 2, 0, 0, 500000},

    // Large divisors
    {1000000, 0, 1000000, 0, 0, 1},
    {1000000, 0, 999999, 0, 0, 1},
    {1000000, 0, 1000001, 0, 0, 0},
};
INSTANTIATE_TEST_SUITE_P(integers, FixedDivideTest, ValuesIn(divide_integers));

/*
  All Sign Combinations

  Verifies correct sign handling for quotients: positive/positive = positive,
  positive/negative = negative, negative/positive = negative,
  negative/negative = positive.
*/
const DivideTestParams divide_signs[] = {
    // Positive / Positive = Positive
    {100, 0, 2, 0, 0, 50},
    {1000LL << 32, 32, 10LL << 32, 32, 32, 100LL << 32},

    // Positive / Negative = Negative
    {100, 0, -2, 0, 0, -50},
    {1000LL << 32, 32, -(10LL << 32), 32, 32, -(100LL << 32)},

    // Negative / Positive = Negative
    {-100, 0, 2, 0, 0, -50},
    {-(1000LL << 32), 32, 10LL << 32, 32, 32, -(100LL << 32)},

    // Negative / Negative = Positive
    {-100, 0, -2, 0, 0, 50},
    {-(1000LL << 32), 32, -(10LL << 32), 32, 32, 100LL << 32},

    // Edge: Dividing by -1 negates
    {1234, 0, -1, 0, 0, -1234},
    {-1234, 0, -1, 0, 0, 1234},
    {5678LL << 16, 16, -(1LL << 16), 16, 16, -(5678LL << 16)},
};
INSTANTIATE_TEST_SUITE_P(signs, FixedDivideTest, ValuesIn(divide_signs));

/*
  Output Precision Greater Than Input Precision

  Tests where output_frac_bits > dividend_frac_bits + optimal_shift -
  divisor_frac_bits, requiring a left shift of the quotient.
*/
const DivideTestParams divide_precision_upscale[] = {
    // Integer to fixed-point
    {1, 0, 2, 0, 1, 1},           // 0.5 in Q0.1
    {1, 0, 2, 0, 16, 1 << 15},    // 0.5 in Q16
    {1, 0, 2, 0, 32, 1LL << 31},  // 0.5 in Q32
    {3, 0, 4, 0, 16, 49152},      // 0.75 in Q16

    // Low precision to high precision
    {100LL << 8, 8, 10LL << 8, 8, 32, 10LL << 32},
    {50LL << 16, 16, 5LL << 16, 16, 48, 10LL << 48},

    // Mixed input precisions, high output
    {100, 0, 1LL << 16, 16, 32, 100LL << 32},
    {1LL << 16, 16, 100, 0, 32, (1LL << 32) / 100},
};
INSTANTIATE_TEST_SUITE_P(precision_upscale, FixedDivideTest,
                         ValuesIn(divide_precision_upscale));

/*
  Output Precision Less Than Input Precision

  Tests where:
    output_frac_bits < dividend_frac_bits + optimal_shift - divisor_frac_bits,
  requiring a right shift of the quotient with round-to-zero behavior.
*/
const DivideTestParams divide_precision_downscale[] = {
    // High precision to integer
    {100LL << 32, 32, 10LL << 32, 32, 0, 10},
    {(1LL << 32) / 2, 32, 1LL << 32, 32, 0, 0},  // 0.5 truncates to 0
    {(3LL << 32) / 2, 32, 1LL << 32, 32, 0, 1},  // 1.5 truncates to 1

    // High to mid precision
    {100LL << 48, 48, 10LL << 48, 48, 32, 10LL << 32},
    {100LL << 48, 48, 10LL << 48, 48, 16, 10LL << 16},

    // Mid to low precision
    {100LL << 32, 32, 10LL << 32, 32, 16, 10LL << 16},
    {100LL << 32, 32, 10LL << 32, 32, 8, 10LL << 8},

    // Precision loss with rounding
    {1001LL << 32, 32, 1000LL << 32, 32, 0, 1},  // 1.001 -> 1
    {999LL << 32, 32, 1000LL << 32, 32, 0, 0},   // 0.999 -> 0
};
INSTANTIATE_TEST_SUITE_P(precision_downscale, FixedDivideTest,
                         ValuesIn(divide_precision_downscale));

/*
  All Precisions Equal

  When dividend_frac_bits = divisor_frac_bits = output_frac_bits, the
  scaling should be relatively straightforward.
*/
const DivideTestParams divide_equal_precision[] = {
    // Q32.32 format
    {100LL << 32, 32, 10LL << 32, 32, 32, 10LL << 32},
    {(3LL << 32) / 2, 32, 1LL << 32, 32, 32, (3LL << 32) / 2},

    // Q48.16 format
    {100LL << 16, 16, 10LL << 16, 16, 16, 10LL << 16},
    {1000LL << 16, 16, 3LL << 16, 16, 16, (333LL << 16) + 21845},

    // Q61.2 format (high precision)
    {100LL << 2, 2, 10LL << 2, 2, 2, 10LL << 2},
    {7LL << 2, 2, 2LL << 2, 2, 2, (7LL << 2) / 2},

    // Q0.0 format (integers)
    {1000, 0, 10, 0, 0, 100},
};
INSTANTIATE_TEST_SUITE_P(equal_precision, FixedDivideTest,
                         ValuesIn(divide_equal_precision));

/*
  Optimal Shift Equals Zero

  When the dividend is large enough that no left shift can be applied without
  risking overflow. This happens when clz(dividend) <= clz(divisor) + 1.
*/
const DivideTestParams divide_optimal_shift_zero[] = {
    // S64_MAX / 1: clz(MAX) = 1, clz(1) = 63, optimal = 62 + 1 - 63 = 0
    {S64_MAX, 0, 1, 0, 0, S64_MAX},

    // Large dividend / small divisor
    {1LL << 62, 0, 1, 0, 0, 1LL << 62},
    {(1LL << 62) + 1, 0, 1, 0, 0, (1LL << 62) + 1},

    // With fractional bits
    {S64_MAX, 0, 1, 0, 16, S64_MAX},  // Saturates
    {1LL << 61, 0, 1, 0, 1, 1LL << 62},

    // Negative cases
    {-(1LL << 62), 0, 1, 0, 0, -(1LL << 62)},
    {S64_MIN >> 1, 0, 1, 0, 0, S64_MIN >> 1},
};
INSTANTIATE_TEST_SUITE_P(optimal_shift_zero, FixedDivideTest,
                         ValuesIn(divide_optimal_shift_zero));

/*
  Optimal Shift Equals Negative One

  The special S64_MIN / 1 case where clz(S64_MIN) = 0, resulting in
  optimal_shift = 62 + 0 - 63 = -1, which should saturate.
*/
const DivideTestParams divide_optimal_shift_negative[] = {
    // S64_MIN / 1 should saturate to S64_MIN (same sign)
    {S64_MIN, 0, 1, 0, 0, S64_MIN},

    // S64_MIN / -1 should saturate to S64_MAX (different signs)
    {S64_MIN, 0, -1, 0, 0, S64_MAX},

    // With fractional bits
    {S64_MIN, 0, 1, 0, 32, S64_MIN},
    {S64_MIN, 0, -1, 0, 32, S64_MAX},
    {S64_MIN, 32, 1LL << 32, 32, 32, S64_MIN},
};
INSTANTIATE_TEST_SUITE_P(optimal_shift_negative, FixedDivideTest,
                         ValuesIn(divide_optimal_shift_negative));

/*
  High Optimal Shift Values

  Small dividend and/or large divisor produce high optimal shift values,
  maximizing intermediate precision before division.
*/
const DivideTestParams divide_optimal_shift_maximum[] = {
    // 1 / S64_MAX: clz(1) = 63, clz(MAX) = 1, optimal = 62 + 63 - 1 = 124
    {1, 0, S64_MAX, 0, 0, 0},
    {1, 0, S64_MAX, 0, 32, 0},
    {1, 0, S64_MAX, 0, 62, 0},

    // Small / Large with output precision
    {1, 0, 1LL << 50, 0, 62, 4096},     // 2^-50 in Q62 = 2^12 = 4096
    {1, 0, 1LL << 40, 0, 62, 4194304},  // 2^-40 in Q62 = 2^22

    // Precise small divisions
    {1, 0, 3, 0, 62, 1537228672809129301LL},  // 1/3 in Q62
    {1, 0, 7, 0, 62, 658812288346769700LL},   // 1/7 in Q62, rounded toward 0
};
INSTANTIATE_TEST_SUITE_P(optimal_shift_maximum, FixedDivideTest,
                         ValuesIn(divide_optimal_shift_maximum));

/*
  Remaining Shift Exceeds 63

  When remaining_shift > 63, the result would require shifting beyond s64
  capacity, so the function saturates based on quotient sign.
*/
const DivideTestParams divide_remaining_shift_overflow[] = {
    // Positive results saturate to S64_MAX
    {1, 0, 1LL << 62, 62, 62, 1LL << 62},
    {100, 0, 1, 0, 63, S64_MAX},
    {1, 0, 2, 1, 63, S64_MAX},

    // Negative results saturate to S64_MIN
    {-1, 0, 1LL << 62, 62, 62, -1LL << 62},
    {-100, 0, 1, 0, 63, S64_MIN},
    {-1, 0, 2, 1, 63, S64_MIN},
    {1, 0, -2, 1, 63, S64_MIN},
};
INSTANTIATE_TEST_SUITE_P(remaining_shift_overflow, FixedDivideTest,
                         ValuesIn(divide_remaining_shift_overflow));

/*
  Remaining Shift Below -63

  When remaining_shift < -63, the result would be shifted so far right that
  it becomes 0.
*/
const DivideTestParams divide_remaining_shift_underflow[] = {
    // Large intermediate precision, no output precision
    {1, 62, S64_MAX, 0, 0, 0},
    {100, 60, 1LL << 62, 0, 0, 0},
    {1LL << 62, 62, 1, 0, 0, 1},

    // Various combinations that produce massive right shifts
    {1, 60, 1LL << 60, 0, 0, 0},
    {1, 62, 1LL << 30, 0, 0, 0},
};
INSTANTIATE_TEST_SUITE_P(remaining_shift_underflow, FixedDivideTest,
                         ValuesIn(divide_remaining_shift_underflow));

/*
  Rounding Positive Fractions

  When a positive result has a fractional part, it should truncate toward zero.
*/
const DivideTestParams divide_rtz_positive[] = {
    // 1 / 3 = 0.333...
    {1, 0, 3, 0, 0, 0},
    {1, 0, 3, 0, 16, 21845},       // 0.333328... in Q16
    {1, 0, 3, 0, 32, 1431655765},  // 0.333333... in Q32

    // 2 / 3 = 0.666...
    {2, 0, 3, 0, 0, 0},
    {2, 0, 3, 0, 16, 43690},

    // 7 / 4 = 1.75
    {7, 0, 4, 0, 0, 1},
    {7, 0, 4, 0, 16, 114688},

    // 99 / 100 = 0.99
    {99, 0, 100, 0, 0, 0},
    {99, 0, 100, 0, 16, 64880},

    // 1001 / 1000 = 1.001
    {1001, 0, 1000, 0, 0, 1},
    {1001, 0, 1000, 0, 16, 65601},

    // Very small fractions
    {1, 0, 1000000, 0, 0, 0},
    {1, 0, 1000000, 0, 32, 4294},
};
INSTANTIATE_TEST_SUITE_P(rtz_positive, FixedDivideTest,
                         ValuesIn(divide_rtz_positive));

/*
  Rounding Negative Fractions

  When a negative result has a fractional part, it should truncate toward zero
  (not toward negative infinity like floor division).
*/
const DivideTestParams divide_rtz_negative[] = {
    // -1 / 3 = -0.333...
    {-1, 0, 3, 0, 0, 0},
    {-1, 0, 3, 0, 16, -21845},
    {1, 0, -3, 0, 16, -21845},

    // -2 / 3 = -0.666...
    {-2, 0, 3, 0, 0, 0},
    {-2, 0, 3, 0, 16, -43690},
    {2, 0, -3, 0, 16, -43690},

    // -7 / 4 = -1.75
    {-7, 0, 4, 0, 0, -1},
    {-7, 0, 4, 0, 16, -114688},

    // -99 / 100 = -0.99
    {-99, 0, 100, 0, 0, 0},
    {-99, 0, 100, 0, 16, -64880},

    // -1001 / 1000 = -1.001
    {-1001, 0, 1000, 0, 0, -1},
    {-1001, 0, 1000, 0, 16, -65601},
};
INSTANTIATE_TEST_SUITE_P(rtz_negative, FixedDivideTest,
                         ValuesIn(divide_rtz_negative));

/*
  Results That Round to Zero

  Division operations where the mathematical result is very small and rounds
  to zero at the requested output precision.
*/
const DivideTestParams divide_near_zero[] = {
    // Positive near-zero
    {1, 0, 1000000, 0, 0, 0},
    {1, 0, S64_MAX, 0, 0, 0},
    {1, 0, 1LL << 62, 0, 0, 0},
    {1, 32, S64_MAX, 32, 32, 0},

    // Negative near-zero
    {-1, 0, 1000000, 0, 0, 0},
    {-1, 0, S64_MAX, 0, 0, 0},
    {1, 0, -1000000, 0, 0, 0},

    // Small dividend, large divisor, various precisions
    {1, 0, 1LL << 50, 0, 10, 0},
    {10, 0, 1LL << 50, 0, 10, 0},
    {100, 16, 1LL << 50, 16, 16, 0},
};
INSTANTIATE_TEST_SUITE_P(near_zero, FixedDivideTest,
                         ValuesIn(divide_near_zero));

/*
  Positive Results That Overflow

  Division operations that would produce results exceeding S64_MAX.
*/
const DivideTestParams divide_saturate_positive[] = {
    // S64_MAX / small divisor with precision increase
    {S64_MAX, 0, 1, 0, 1, S64_MAX},
    {S64_MAX, 0, 1, 0, 10, S64_MAX},
    {S64_MAX, 0, 2, 0, 1, S64_MAX},

    // Large / small with high output precision
    {1LL << 62, 0, 1, 0, 1, S64_MAX},
    {1LL << 61, 0, 1, 0, 2, S64_MAX},

    // Near-boundary cases
    {(1LL << 62) - 1, 0, 1, 0, 1, ((1LL << 62) - 1) << 1},

    // With fractional bits
    {S64_MAX, 32, 1LL << 32, 32, 33, S64_MAX},
    {1LL << 62, 32, 1LL << 32, 32, 33, S64_MAX},
};
INSTANTIATE_TEST_SUITE_P(saturate_positive, FixedDivideTest,
                         ValuesIn(divide_saturate_positive));

/*
  Negative Results That Overflow

  Division operations that would produce results below S64_MIN.
*/
const DivideTestParams divide_saturate_negative[] = {
    // S64_MIN / 1 with precision increase
    {S64_MIN, 0, 1, 0, 1, S64_MIN},
    {S64_MIN, 0, 1, 0, 10, S64_MIN},

    // S64_MIN / -1 (special case)
    {S64_MIN, 0, -1, 0, 0, S64_MAX},
    {S64_MIN, 0, -1, 0, 1, S64_MAX},

    // Large negative / small divisor
    {-(1LL << 62), 0, 1, 0, 1, S64_MIN},
    {S64_MIN, 0, 2, 0, 1, S64_MIN},

    // Mixed signs
    {S64_MAX, 0, -1, 0, 1, S64_MIN},
    {-(1LL << 62), 0, 1, 0, 2, S64_MIN},

    // With fractional bits
    {S64_MIN, 32, 1LL << 32, 32, 33, S64_MIN},
    {-(1LL << 62), 32, 1LL << 32, 32, 33, S64_MIN},
};
INSTANTIATE_TEST_SUITE_P(saturate_negative, FixedDivideTest,
                         ValuesIn(divide_saturate_negative));

/*
  S64_MIN Edge Cases

  S64_MIN is special because it's the only value where abs(x) doesn't fit
  in the positive range of s64. Tests various operations involving S64_MIN.
*/
const DivideTestParams divide_s64_min_special[] = {
    // S64_MIN / 1 = S64_MIN
    {S64_MIN, 0, 1, 0, 0, S64_MIN},
    {S64_MIN, 32, 1LL << 32, 32, 32, S64_MIN},

    // S64_MIN / -1 = overflow to S64_MAX
    {S64_MIN, 0, -1, 0, 0, S64_MAX},
    {S64_MIN, 32, -(1LL << 32), 32, 32, S64_MAX},

    // S64_MIN / 2 = -(1 << 62)
    {S64_MIN, 0, 2, 0, 0, -(1LL << 62)},
    {S64_MIN, 32, 2LL << 32, 32, 32, -(1LL << 62)},

    // S64_MIN / -2 = 1 << 62
    {S64_MIN, 0, -2, 0, 0, 1LL << 62},

    // S64_MIN / S64_MAX ~= -1
    {S64_MIN, 0, S64_MAX, 0, 0, -1},
    {S64_MIN, 32, S64_MAX, 32, 32, -(1LL << 32)},

    // S64_MIN / S64_MIN = 1
    {S64_MIN, 0, S64_MIN, 0, 0, 1},
    {S64_MIN, 32, S64_MIN, 32, 32, 1LL << 32},
};
INSTANTIATE_TEST_SUITE_P(s64_min_special, FixedDivideTest,
                         ValuesIn(divide_s64_min_special));

/*
  S64_MAX Edge Cases

  Tests involving the maximum positive s64 value.
*/
const DivideTestParams divide_s64_max_cases[] = {
    // S64_MAX / 1 = S64_MAX
    {S64_MAX, 0, 1, 0, 0, S64_MAX},
    {S64_MAX, 32, 1LL << 32, 32, 32, S64_MAX},

    // S64_MAX / -1 = -S64_MAX
    {S64_MAX, 0, -1, 0, 0, -S64_MAX},

    // S64_MAX / 2
    {S64_MAX, 0, 2, 0, 0, S64_MAX >> 1},
    {S64_MAX, 32, 2LL << 32, 32, 32, (S64_MAX >> 1)},

    // S64_MAX / S64_MAX = 1
    {S64_MAX, 0, S64_MAX, 0, 0, 1},
    {S64_MAX, 32, S64_MAX, 32, 32, 1LL << 32},

    // S64_MAX / S64_MIN ~= -1
    {S64_MAX, 0, S64_MIN, 0, 0, 0},  // Actually -1 < result < 0, rounds to 0

    // 1 / S64_MAX ~= 0
    {1, 0, S64_MAX, 0, 0, 0},
    {1, 0, S64_MAX, 0, 62, 0},
};
INSTANTIATE_TEST_SUITE_P(s64_max_cases, FixedDivideTest,
                         ValuesIn(divide_s64_max_cases));

/*
  Maximum Safe Precision

  Tests using the highest safe fractional bit counts for the input values.
*/
const DivideTestParams divide_high_precision[] = {
    // 1.5 / 1.5 = 1.0
    {(1LL << 62) + (1LL << 61), 62, (1LL << 62) + (1LL << 61), 62, 62,
     1LL << 62},

    // 0.875 / 0.375 = 2.333... (7/3, scaled down)
    {7LL << 59, 62, 3LL << 59, 60, 60, (7LL << 58) / 3},

    // 1.5 / 0.5 = 3.0
    {(1LL << 62) + (1LL << 61), 62, (1LL << 61), 62, 32, 3LL << 32},

    // 1.0 / 3.0 = 0.333...
    {1LL << 61, 61, 3LL << 61, 61, 16, (1LL << 16) / 3},

    // Lower precision inputs, high output - these work if result fits
    {15, 0, 10, 0, 62, (3LL << 61)},  // 15/10 = 1.5
    {7, 0, 4, 0, 62, (7LL << 60)},    // 7/4 = 1.75
};
INSTANTIATE_TEST_SUITE_P(high_precision, FixedDivideTest,
                         ValuesIn(divide_high_precision));

/*
  Practical Real-World Cases

  Division operations that might appear in actual fixed-point calculations,
  with realistic precision combinations.
*/
const DivideTestParams divide_realistic[] = {
    // Computing percentages (Q16.16)
    {75LL << 16, 16, 100LL << 16, 16, 16, (75LL << 16) / 100},        // 0.75
    {12345LL << 16, 16, 100LL << 16, 16, 16, (12345LL << 16) / 100},  // 123.45

    // Frame rates and time (Q32.32)
    {1LL << 32, 32, 60LL << 32, 32, 32, (1LL << 32) / 60},        // 1/60 second
    {1000LL << 32, 32, 16LL << 32, 32, 32, (1000LL << 32) / 16},  // 62.5

    // Physics calculations (Q24.40)
    {98LL << 40, 40, 10LL << 40, 40, 40, (98LL << 40) / 10},  // 9.8 m/s^2

    // Financial (Q16.48)
    {100LL << 48, 48, 3LL << 48, 48, 48, (100LL << 48) / 3},  // 33.333...

    // Graphics/normalized values (Q2.61)
    {1LL << 61, 61, 2LL << 61, 61, 61, 1LL << 60},  // 0.5

    // Mixed precision realistic
    {100LL << 16, 16, 3, 0, 32, (100LL << 32) / 3},
    {1000, 0, 16LL << 16, 16, 16, (1000LL << 16) / 16},
};
INSTANTIATE_TEST_SUITE_P(realistic, FixedDivideTest,
                         ValuesIn(divide_realistic));
#endif

}  // namespace
}  // namespace curves
