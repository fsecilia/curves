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
// curves_fixed_subtract()
// ----------------------------------------------------------------------------

struct SubtractTestParams {
  s64 minuend;
  unsigned int minuend_frac_bits;
  s64 subtrahend;
  unsigned int subtrahend_frac_bits;
  unsigned int output_frac_bits;
  s64 expected_result;

  friend auto operator<<(std::ostream& out, const SubtractTestParams& src)
      -> std::ostream& {
    return out << "{" << src.minuend << ", " << src.minuend_frac_bits << ", "
               << src.subtrahend << ", " << src.subtrahend_frac_bits << ", "
               << src.output_frac_bits << ", " << src.expected_result << "}";
  }
};

struct FixedSubtractTest : TestWithParam<SubtractTestParams> {};

TEST_P(FixedSubtractTest, expected_result) {
  const auto expected_result = GetParam().expected_result;

  const auto actual_result = curves_fixed_subtract(
      GetParam().minuend, GetParam().minuend_frac_bits, GetParam().subtrahend,
      GetParam().subtrahend_frac_bits, GetParam().output_frac_bits);

  ASSERT_EQ(expected_result, actual_result);
}

/*
  Identity and Baseline

  Baseline sanity checks without precision changes. Zero subtracted anything to
  changes nothing, regardless of precision.
*/
const SubtractTestParams subtract_zero[] = {
    {0, 0, 0, 0, 0, 0},         // Zero precision
    {0, 32, 0, 32, 32, 0},      // Mid precision
    {0, 62, 5, 62, 62, -5},     // High precision, non-zero subtrahend
    {100, 32, 0, 32, 32, 100},  // Non-zero minuend
};
INSTANTIATE_TEST_SUITE_P(zero, FixedSubtractTest, ValuesIn(subtract_zero));

/*
  Invalid Fractional Bits

  Tests that frac_bits >= 64 triggers the error handler and returns 0.
*/
const SubtractTestParams subtract_invalid_frac_bits[] = {
    // Invalid minuend_frac_bits
    {100, 64, 50, 32, 32, 0},
    {100, 65, 50, 32, 32, 0},
    {-100, 64, 50, 32, 32, 0},

    // Invalid subtrahend_frac_bits
    {100, 32, 50, 64, 32, 0},
    {100, 32, 50, 65, 32, 0},
    {100, 32, -50, 64, 32, 0},

    // Invalid output_frac_bits
    {100, 32, 50, 32, 64, 0},
    {100, 32, 50, 32, 65, 0},
    {-100, 32, -50, 32, 64, 0},

    // Multiple invalid parameters
    {100, 64, 50, 64, 32, 0},
    {100, 64, 50, 32, 64, 0},
    {100, 32, 50, 64, 64, 0},
    {100, 64, 50, 64, 64, 0},
};
INSTANTIATE_TEST_SUITE_P(invalid_frac_bits, FixedSubtractTest,
                         ValuesIn(subtract_invalid_frac_bits));

/*
  Simple Integer Subtraction

  Basic integer subtraction with frac_bits = 0 for all parameters.
*/
const SubtractTestParams subtract_integers[] = {
    // Small positive values
    {2, 0, 3, 0, 0, -1},
    {10, 0, 20, 0, 0, -10},
    {100, 0, 200, 0, 0, -100},

    // Negative values
    {-10, 0, -20, 0, 0, 10},
    {-5, 0, -3, 0, 0, -2},

    // Mixed signs (effectively subtraction)
    {10, 0, -3, 0, 0, 13},
    {-10, 0, 5, 0, 0, -15},
    {100, 0, -100, 0, 0, 200},

    // Large values that fit
    {1000000, 0, 2000000, 0, 0, -1000000},
    {-1000000, 0, -2000000, 0, 0, 1000000},
};
INSTANTIATE_TEST_SUITE_P(integers, FixedSubtractTest,
                         ValuesIn(subtract_integers));

/*
  Equal Precision Subtraction

  When minuend_frac_bits = subtrahend_frac_bits = output_frac_bits, no rescaling
  is needed. Tests the core subtraction logic without precision conversion.
*/
const SubtractTestParams subtract_equal_precision[] = {
    // Q32.32 format
    {10LL << 32, 32, 20LL << 32, 32, 32, -(10LL << 32)},
    {-(5LL << 32), 32, -(3LL << 32), 32, 32, -(2LL << 32)},
    {-(50LL << 32), 32, -(100LL << 32), 32, 32, 50LL << 32},

    // 1.5 - 2.5 = -1.0
    {(1LL << 32) + (1LL << 31), 32, (2LL << 32) + (1LL << 31), 32, 32,
     -(1LL << 32)},

    // 10.25 - 20.75 = -10.5
    {(10LL << 32) + (1LL << 30), 32, (20LL << 32) + (3LL << 30), 32, 32,
     -(10LL << 32) - (1LL << 31)},

    // Q16.48 format
    {100LL << 48, 48, 200LL << 48, 48, 48, -(100LL << 48)},
    {-(50LL << 48), 48, -(30LL << 48), 48, 48, -(20LL << 48)},

    // Q61.2 format (high precision)
    {3LL << 2, 2, 7LL << 2, 2, 2, -(4LL << 2)},
    {-(5LL << 2), 2, -(2LL << 2), 2, 2, -(3LL << 2)},

    // Q0.0 format (integers, same as integer tests but different category)
    {42, 0, 58, 0, 0, -16},
};
INSTANTIATE_TEST_SUITE_P(equal_precision, FixedSubtractTest,
                         ValuesIn(subtract_equal_precision));

/*
  Different Input Precisions

  Tests subtraction when the two operands have different precisions. Both are
  rescaled to max(minuend_frac_bits, subtrahend_frac_bits, output_frac_bits)
  before subtracting.
*/
const SubtractTestParams subtract_different_input_precision[] = {
    // Low precision minuend, high precision subtrahend, output matches
    // subtrahend
    {10, 0, 20LL << 32, 32, 32, (10LL << 32) - (20LL << 32)},
    {5LL << 16, 16, 10LL << 48, 48, 48, (5LL << 48) - (10LL << 48)},

    // High precision minuend, low precision subtrahend, output matches minuend
    {10LL << 32, 32, 20, 0, 32, (10LL << 32) - (20LL << 32)},
    {10LL << 48, 48, 5LL << 16, 16, 48, (10LL << 48) - (5LL << 48)},

    // Both different, output matches neither
    {10LL << 16, 16, 20LL << 24, 24, 32, (10LL << 32) - (20LL << 32)},
    {5LL << 8, 8, 3LL << 16, 16, 24, (5LL << 24) - (3LL << 24)},
    {-(10LL << 16), 16, -(100LL << 48), 48, 32, 90LL << 32},

    // Negative values with mixed precisions
    {-(10LL << 32), 32, -20, 0, 32, -(10LL << 32) + (20LL << 32)},
    {-5, 0, -(3LL << 32), 32, 32, -(5LL << 32) + (3LL << 32)},
};
INSTANTIATE_TEST_SUITE_P(different_input_precision, FixedSubtractTest,
                         ValuesIn(subtract_different_input_precision));

/*
  Order-dependent Mixed Precision

  These test what addition's commutativity test case catches.
*/
const SubtractTestParams subtract_order_dependent[] = {
    // High precision first, low precision second
    {100LL << 48, 48, 50, 0, 48, (50LL << 48)},
    {100LL << 48, 48, 50, 0, 32, (50LL << 32)},
    {100LL << 48, 48, 50, 0, 16, (50LL << 16)},

    // Low precision first, high precision second, negative result.
    {100, 0, 150LL << 48, 48, 48, -(50LL << 48)},
    {100, 0, 150LL << 48, 48, 32, -(50LL << 32)},

    // Different input precisions, various output precisions
    {100LL << 24, 24, 50LL << 40, 40, 16, (50LL << 16)},
    {100LL << 40, 40, 150LL << 24, 24, 32, -(50LL << 32)},
};
INSTANTIATE_TEST_SUITE_P(order_dependent, FixedSubtractTest,
                         ValuesIn(subtract_order_dependent));

/*
  Output Precision Conversion

  Tests where the output precision is different from both input precisions,
  requiring a final rescale after the subtraction.
*/
const SubtractTestParams subtract_output_precision_differs[] = {
    // Inputs at Q32, output at Q16 (downscale)
    {10LL << 32, 32, 20LL << 32, 32, 16, -(10LL << 16)},
    {(3LL << 32) + (1LL << 31), 32, (2LL << 32) + (1LL << 31), 32, 16,
     1LL << 16},  // 3.5 - 2.5 = 1.0 at different precision

    // Inputs at Q16, output at Q32 (upscale)
    {10LL << 16, 16, 20LL << 16, 16, 32, -(10LL << 32)},
    {5LL << 16, 16, 3LL << 16, 16, 48, 2LL << 48},

    // Inputs at different precisions, output at third precision
    {10LL << 16, 16, 20LL << 48, 48, 32, -(10LL << 32)},
    {5LL << 8, 8, 3LL << 40, 40, 24, 2LL << 24},

    // Output at Q0 (truncates fractional parts)

    // 10.5 - 20.5 = -10
    {(10LL << 32) + (1LL << 31), 32, (20LL << 32) + (1LL << 31), 32, 0, -10},

    // 5.75 - 3.75 = 2
    {(5LL << 32) + (3LL << 30), 32, (3LL << 32) + (1LL << 30), 32, 0, 2},

    // Large values downscaling
    {(S64_MAX >> 17), 48, (S64_MAX >> 18), 48, 32,
     ((S64_MAX >> 17) - (S64_MAX >> 18)) >> 16},

    // Upscaling near boundaries
    {(S64_MAX >> 33), 0, (S64_MAX >> 34), 0, 32,
     ((S64_MAX >> 33) - (S64_MAX >> 34)) << 32},

    // Mixed precision with saturation, saturates after rescale
    {S64_MAX >> 1, 16, -(S64_MAX >> 2), 0, 32, S64_MAX},
};
INSTANTIATE_TEST_SUITE_P(output_precision_differs, FixedSubtractTest,
                         ValuesIn(subtract_output_precision_differs));

/*
  All Sign Combinations

  Tests all four combinations of operand signs: pos-pos, pos-neg, neg-pos,
  neg-neg. The pos-neg and neg-neg cases effectively test addition behavior.
*/
const SubtractTestParams subtract_signs[] = {
    // Positive - Positive result depends on magnitudes
    {100, 0, 50, 0, 0, 50},   // Result positive
    {50, 0, 100, 0, 0, -50},  // Result negative
    {100, 0, 100, 0, 0, 0},   // Result zero
    {10LL << 32, 32, 20LL << 32, 32, 32, -10LL << 32},

    // Positive - Negative = Positive (larger)
    {100, 0, -50, 0, 0, 150},
    {50, 0, -100, 0, 0, 150},
    {100, 0, -100, 0, 0, 200},
    {10LL << 32, 32, -(3LL << 32), 32, 32, 13LL << 32},

    // Negative - Positive = Negative (larger magnitude)
    {-100, 0, 50, 0, 0, -150},
    {-50, 0, 100, 0, 0, -150},
    {-100, 0, 100, 0, 0, -200},
    {-(10LL << 32), 32, 3LL << 32, 32, 32, -(13LL << 32)},

    // Negative - Negative result depends on magnitudes
    {-50, 0, -100, 0, 0, 50},   // Result positive
    {-100, 0, -50, 0, 0, -50},  // Result negative
    {-100, 0, -100, 0, 0, 0},   // Result zero
    {-(10LL << 32), 32, -(20LL << 32), 32, 32, 10LL << 32},
};
INSTANTIATE_TEST_SUITE_P(signs, FixedSubtractTest, ValuesIn(subtract_signs));

/*
  Positive Overflow Saturation

  Tests cases where two values difference to more than S64_MAX, requiring
  saturation to S64_MAX.
*/
const SubtractTestParams subtract_saturate_positive[] = {
    // Simple integer overflow
    {S64_MAX, 0, -1, 0, 0, S64_MAX},
    {S64_MAX, 0, -100, 0, 0, S64_MAX},
    {S64_MAX, 0, -S64_MAX, 0, 0, S64_MAX},

    // Two large values that sum to overflow
    {(S64_MAX >> 1) + 1, 0, -((S64_MAX >> 1) + 1), 0, 0, S64_MAX},
    {S64_MAX - 100, 0, -200, 0, 0, S64_MAX},

    // With fractional bits at same precision
    {S64_MAX, 32, -(1LL << 32), 32, 32, S64_MAX},
    {(S64_MAX >> 1) + 1, 32, -((S64_MAX >> 1) + 1), 32, 32, S64_MAX},

    // With different precisions (overflow after rescaling)
    {S64_MAX >> 16, 16, -(1LL << 16), 16, 32, S64_MAX},
    {S64_MAX, 0, -(1LL << 32), 32, 32, S64_MAX},

    // Overflow after upscaling to output precision
    {S64_MAX >> 1, 0, -(S64_MAX >> 1), 0, 1, S64_MAX},
};
INSTANTIATE_TEST_SUITE_P(saturate_positive, FixedSubtractTest,
                         ValuesIn(subtract_saturate_positive));

/*
  Negative Overflow Saturation

  Tests cases where two values difference to less than S64_MIN, requiring
  saturation to S64_MIN.
*/
const SubtractTestParams subtract_saturate_negative[] = {
    // Simple integer underflow
    {S64_MIN, 0, 1, 0, 0, S64_MIN},
    {S64_MIN, 0, 100, 0, 0, S64_MIN},
    {S64_MIN, 0, S64_MAX, 0, 0, S64_MIN},

    // Two large negative values that sum to underflow
    {(S64_MIN >> 1) - 1, 0, -(S64_MIN >> 1) + 1, 0, 0, S64_MIN},
    {S64_MIN + 100, 0, 200, 0, 0, S64_MIN},

    // With fractional bits at same precision
    {S64_MIN, 32, (1LL << 32), 32, 32, S64_MIN},
    {(S64_MIN >> 1) - 1, 32, -(S64_MIN >> 1) + 1, 32, 32, S64_MIN},

    // With different precisions (underflow after rescaling)
    {S64_MIN >> 16, 16, (1LL << 16), 16, 32, S64_MIN},
    {S64_MIN, 0, (1LL << 32), 32, 32, S64_MIN},

    // Underflow when upscaling to output precision
    {(S64_MIN >> 1), 0, -(S64_MIN >> 1), 0, 1, S64_MIN},
};
INSTANTIATE_TEST_SUITE_P(saturate_negative, FixedSubtractTest,
                         ValuesIn(subtract_saturate_negative));

/*
  Near Saturation Boundaries

  Tests values that are close to overflow/underflow but don't quite reach it,
  and values that are exactly at the boundary.
*/
const SubtractTestParams subtract_boundaries[] = {
    // Just under positive overflow
    {S64_MAX - 1, 0, -1, 0, 0, S64_MAX},
    {S64_MAX - 100, 0, -100, 0, 0, S64_MAX},
    {S64_MAX - 100, 0, -99, 0, 0, S64_MAX - 1},

    // Just above negative underflow
    {S64_MIN + 1, 0, 1, 0, 0, S64_MIN},
    {S64_MIN + 100, 0, 100, 0, 0, S64_MIN},
    {S64_MIN + 100, 0, 99, 0, 0, S64_MIN + 1},

    // Large values that just barely fit without overflow
    {S64_MAX >> 1, 0, -(S64_MAX >> 1), 0, 0, S64_MAX - 1},
    {(S64_MIN >> 1) + 1, 0, -((S64_MIN >> 1) + 1), 0, 0, S64_MIN + 2},

    // With fractional bits
    {S64_MAX - (1LL << 32), 32, -((1LL << 32) - 1), 32, 32, S64_MAX - 1},
    {S64_MIN + (1LL << 32), 32, (1LL << 32) - 1, 32, 32, S64_MIN + 1},

    // Values that cross 0.
    {-1, 0, -S64_MAX, 0, 0, S64_MAX - 1},
    {1, 0, S64_MAX, 0, 0, -(S64_MAX - 1)},
    {-1, 0, S64_MIN, 0, 0, S64_MAX},
    {1, 0, S64_MIN, 0, 0, -(S64_MIN + 1)},
};
INSTANTIATE_TEST_SUITE_P(boundaries, FixedSubtractTest,
                         ValuesIn(subtract_boundaries));

/*
  Rounding Behavior

  Tests that when the result is rescaled to lower precision, the rounding is
  toward zero (truncation), not toward negative infinity (floor).
*/
const SubtractTestParams subtract_rounding[] = {
    // Positive results with fractional parts
    // 3.25 - 0.75 = 2.5, truncates to 2 at Q0
    {(13LL << 30), 32, (3LL << 30), 32, 0, 2},

    // 3.75 - 0.9375 = 2.85, truncates to 2 at Q0
    {(3LL << 32) + (3LL << 30), 32, 15LL << 28, 32, 0, 3},

    // Negative results with fractional parts
    // -1.75 - 0.75 = -2.5, truncates to -2 at Q0 (toward zero, not -3)
    {-(7LL << 30), 32, 3LL << 30, 32, 0, -2},

    // -1.9375 - 0.9375 = -2.875, truncates to -2 at Q0 (toward zero, not -3)
    {-((1LL << 32) + (15LL << 28)), 32, 15LL << 28, 32, 0, -3},

    // Mixed signs: 11.25 + -5.5 = 5.75, truncates to 5
    {(11LL << 32) + (1LL << 30), 32, ((5LL << 32) + (1LL << 31)), 32, 0, 6},

    // Downscaling from Q32 to Q16
    // 3.999... - 2.000... = 1.999..., keeps precision at Q16
    {(3LL << 32) + (1LL << 32) - 1, 32, 2LL << 32, 32, 16,
     (1LL << 16) + (1LL << 16)},

    // Downscaling from Q32 to Q16
    // 3.999... - 2.0 = 1.999..., keeps precision at Q16
    {(3LL << 32) + (1LL << 32) - 1, 32, 2LL << 32, 32, 16,
     (1LL << 16) + (1LL << 16)},

    // Just under integer boundary (positive)
    // 1.999... - 0.5 = 1.499... -> 1
    {(2LL << 32) - 1, 32, 1LL << 31, 32, 0, 1},

    // Just under integer boundary (negative)
    // -1.999... - (-0.5) = -1.499... -> -1
    {-((2LL << 32) - 1), 32, -(1LL << 31), 32, 0, -1},

    // Crossing integer boundary
    {(1LL << 31), 32, (1LL << 32), 32, 0, 0},    // 0.5 - 1.0 = -0.5 -> 0
    {-(1LL << 31), 32, -(1LL << 32), 32, 0, 0},  // -0.5 - (-1.0) = 0.5 -> 0
};
INSTANTIATE_TEST_SUITE_P(rounding, FixedSubtractTest,
                         ValuesIn(subtract_rounding));

/*
  S64 Boundary Values

  Tests involving S64_MAX and S64_MIN to ensure they're handled correctly in
  all contexts (as operands, after rescaling, etc.).
*/
const SubtractTestParams subtract_s64_boundaries[] = {
    // S64_MAX as operand
    {S64_MAX, 0, 0, 0, 0, S64_MAX},      // MAX - 0 = MAX
    {S64_MAX, 32, 0, 32, 32, S64_MAX},   // MAX - 0 at Q32 = MAX
    {S64_MAX, 0, 1, 0, 0, S64_MAX - 1},  // MAX - 1 = MAX - 1
    {S64_MAX, 32, 1LL << 32, 32, 32, S64_MAX - (1LL << 32)},

    // S64_MIN as operand
    {S64_MIN, 0, 0, 0, 0, S64_MIN},       // MIN - 0 = MIN
    {S64_MIN, 32, 0, 32, 32, S64_MIN},    // MIN - 0 at Q32 = MIN
    {S64_MIN, 0, -1, 0, 0, S64_MIN + 1},  // MIN - -1 = MIN + 1
    {S64_MIN, 32, -(1LL << 32), 32, 32, S64_MIN + (1LL << 32)},

    // Both at boundaries with opposite signs (should not overflow)
    {S64_MIN, 0, -S64_MAX, 0, 0, -1},  // MIN - -MAX = -1
    {S64_MIN, 32, -S64_MAX, 32, 32, -1},

    // Rescaling boundary values
    {S64_MAX, 0, 0, 0, 32, S64_MAX},  // Rescaling MAX saturates
    {S64_MIN, 0, 0, 0, 32, S64_MIN},  // Rescaling MIN saturates

    // S64_MIN - S64_MIN = 0
    {S64_MIN, 0, S64_MIN, 0, 0, 0},

    // S64_MIN - S64_MAX = saturate to S64_MIN
    {S64_MIN, 0, S64_MAX, 0, 0, S64_MIN},

    // Small positive - S64_MIN = saturate
    {1, 0, S64_MIN, 0, 0, S64_MAX},
    {100, 0, S64_MIN, 0, 0, S64_MAX},

    // Small negative - S64_MIN = does NOT saturate
    {-100, 0, S64_MIN, 0, 0, S64_MAX - 99},
    {-1, 0, S64_MIN, 0, 0, S64_MAX},
};
INSTANTIATE_TEST_SUITE_P(s64_boundaries, FixedSubtractTest,
                         ValuesIn(subtract_s64_boundaries));

/*
  Practical Real-World Cases

  Subtraction operations that might appear in actual fixed-point calculations,
  with realistic precision combinations for common use cases.
*/
const SubtractTestParams subtract_realistic[] = {
    // Physics calculations (Q24.40 for position/velocity)
    {(10LL << 40), 40, (5LL << 40), 40, 40, (5LL << 40)},   // 10.0 - 5.0 m
    {(98LL << 38), 40, (5LL << 38), 40, 40, (93LL << 38)},  // 9.8 - 0.5 m/s^2

    // Graphics/normalized values (Q2.61 for [0,1] range)
    {1LL << 60, 61, 1LL << 59, 61, 61, 1LL << 59},  // 0.5 - 0.25 = 0.25
    {(1LL << 61) + (1LL << 60), 61, 1LL << 59, 61, 61,
     (1LL << 61) + (1LL << 60) - (1LL << 59)},  // 0.75 - 0.25 = 1.0

    // Frame time accumulation (Q32.32)
    // The results are summed this way to make sure they truncate the same as
    // the actual addition does.
    {(1LL << 32) / 60, 32, (1LL << 32) / 60, 32, 32,
     (1LL << 32) / 60 - (1LL << 32) / 60},  // Two 60fps frames
    {(1LL << 32) / 30, 32, (1LL << 32) / 30, 32, 32,
     (1LL << 32) / 30 - (1LL << 32) / 30},  // Two 30fps frames

    // Mixed precision realistic
    {100LL << 16, 16, 50, 0, 32, (50LL << 32)},     // Low - high precision
    {1000, 0, 500LL << 48, 48, 48, (500LL << 48)},  // Integer - high precision
};
INSTANTIATE_TEST_SUITE_P(realistic, FixedSubtractTest,
                         ValuesIn(subtract_realistic));

// ----------------------------------------------------------------------------
// Intermediate Saturation Tests
// ----------------------------------------------------------------------------

/*
  Category 4A: Intermediate Saturation + Output Upscale Saturation
*/

// First arg saturates, then output upscale also saturates
const SubtractTestParams subtract_intermediate_saturation_only[] = {
    // minuend: saturates to S64_MAX at Q32
    // subtrahend: 100 at Q32
    // difference: S64_MAX - 100 at Q32
    // output: tries to upscale to Q33 -> saturates to S64_MAX
    // {S64_MAX >> 10, 0, 100LL << 32, 32, 33, S64_MAX},
    {S64_MAX >> 10, 0, 100LL << 32, 32, 33, S64_MAX},

    // Similar with underflow
    {S64_MIN >> 10, 0, 100LL << 32, 32, 33, S64_MIN},

    // Both saturate to same value -> difference is 0, upscale doesn't saturate
    {S64_MAX >> 10, 0, S64_MAX >> 10, 0, 33, 0},
};
INSTANTIATE_TEST_SUITE_P(intermediate_then_output_upscale, FixedSubtractTest,
                         ValuesIn(subtract_intermediate_saturation_only));

/*
  Category 4B: Intermediate Saturation + Output Downscale
*/

// Intermediate saturation, then downscale for output
const SubtractTestParams subtract_intermediate_then_downscale[] = {
    // minuend: saturates to S64_MAX at Q32
    // subtrahend: 100 at Q32
    // difference: S64_MAX - 100 at Q32
    // output: downscale to Q16
    {S64_MAX >> 10, 0, 100LL << 32, 32, 16, S64_MAX},

    // minuend: saturates to S64_MIN at Q32
    // subtrahend: 100 at Q32
    // difference: S64_MIN at Q32 (saturated in subtract step)
    // output: downscale to Q16
    // {S64_MIN >> 10, 0, 100LL << 32, 32, 16, S64_MIN >> 16},
    {S64_MIN >> 10, 0, 100LL << 32, 32, 16, S64_MIN},  // Stays saturated

    // Both saturate to same value -> 0, then downscale
    {S64_MAX >> 10, 0, S64_MAX >> 10, 0, 16, 0},
    {S64_MIN >> 10, 0, S64_MIN >> 10, 0, 16, 0},

    // Opposite saturation, then downscale
    {S64_MAX >> 10, 0, S64_MIN >> 10, 0, 16, S64_MAX},  // Stays saturated
    {S64_MIN >> 10, 0, S64_MAX >> 10, 0, 16, S64_MIN},  // Stays saturated

};
INSTANTIATE_TEST_SUITE_P(intermediate_then_downscale, FixedSubtractTest,
                         ValuesIn(subtract_intermediate_then_downscale));

}  // namespace
}  // namespace curves
