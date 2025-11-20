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
// curves_fixed_add()
// ----------------------------------------------------------------------------

struct AddTestParams {
  s64 augend;
  unsigned int augend_frac_bits;
  s64 addend;
  unsigned int addend_frac_bits;
  unsigned int output_frac_bits;
  s64 expected_result;

  friend auto operator<<(std::ostream& out, const AddTestParams& src)
      -> std::ostream& {
    return out << "{" << src.augend << ", " << src.augend_frac_bits << ", "
               << src.addend << ", " << src.addend_frac_bits << ", "
               << src.output_frac_bits << ", " << src.expected_result << "}";
  }
};

struct FixedAddTest : TestWithParam<AddTestParams> {};

TEST_P(FixedAddTest, expected_result) {
  const auto expected_result = GetParam().expected_result;

  const auto actual_result = curves_fixed_add(
      GetParam().augend, GetParam().augend_frac_bits, GetParam().addend,
      GetParam().addend_frac_bits, GetParam().output_frac_bits);

  ASSERT_EQ(expected_result, actual_result);
}

/*
  This switches both addend and augend and their frac bits. Because addition is
  commutative, we can reduce the number of test cases by only including
  combinations, rather than permutations.
*/
TEST_P(FixedAddTest, addition_is_commutative) {
  const auto expected_result = GetParam().expected_result;

  const auto actual_result = curves_fixed_add(
      GetParam().addend, GetParam().addend_frac_bits, GetParam().augend,
      GetParam().augend_frac_bits, GetParam().output_frac_bits);

  ASSERT_EQ(expected_result, actual_result);
}

/*
  Identity and Baseline

  Baseline sanity checks without precision changes. Zero added anything to
  changes nothing, regardless of precision.
*/
const AddTestParams add_zero[] = {
    {0, 0, 0, 0, 0, 0},         // Zero precision
    {0, 32, 0, 32, 32, 0},      // Mid precision
    {0, 62, 5, 62, 62, 5},      // High precision, non-zero addend
    {100, 32, 0, 32, 32, 100},  // Non-zero augend
};
INSTANTIATE_TEST_SUITE_P(zero, FixedAddTest, ValuesIn(add_zero));

/*
  Invalid Fractional Bits

  Tests that frac_bits >= 64 triggers the error handler and returns 0.
*/
const AddTestParams add_invalid_frac_bits[] = {
    // Invalid augend_frac_bits
    {100, 64, 50, 32, 32, 0},
    {100, 65, 50, 32, 32, 0},
    {-100, 64, 50, 32, 32, 0},

    // Invalid addend_frac_bits
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
INSTANTIATE_TEST_SUITE_P(invalid_frac_bits, FixedAddTest,
                         ValuesIn(add_invalid_frac_bits));

/*
  Simple Integer Addition

  Basic integer addition with frac_bits = 0 for all parameters.
*/
const AddTestParams add_integers[] = {
    // Small positive values
    {2, 0, 3, 0, 0, 5},
    {10, 0, 20, 0, 0, 30},
    {100, 0, 200, 0, 0, 300},

    // Negative values
    {-10, 0, -20, 0, 0, -30},
    {-5, 0, -3, 0, 0, -8},

    // Mixed signs (effectively subtraction)
    {10, 0, -3, 0, 0, 7},
    {-10, 0, 5, 0, 0, -5},
    {100, 0, -100, 0, 0, 0},

    // Large values that fit
    {1000000, 0, 2000000, 0, 0, 3000000},
    {-1000000, 0, -2000000, 0, 0, -3000000},
};
INSTANTIATE_TEST_SUITE_P(integers, FixedAddTest, ValuesIn(add_integers));

/*
  Equal Precision Addition

  When augend_frac_bits = addend_frac_bits = output_frac_bits, no rescaling is
  needed. Tests the core addition logic without precision conversion.
*/
const AddTestParams add_equal_precision[] = {
    // Q32.32 format
    {10LL << 32, 32, 20LL << 32, 32, 32, 30LL << 32},
    {-(5LL << 32), 32, -(3LL << 32), 32, 32, -(8LL << 32)},
    {(1LL << 32) + (1LL << 31), 32, (2LL << 32) + (1LL << 31), 32, 32,
     (3LL << 32) + (1LL << 32)},  // 1.5 + 2.5 = 4.0

    // Q16.48 format
    {100LL << 48, 48, 200LL << 48, 48, 48, 300LL << 48},
    {-(50LL << 48), 48, -(30LL << 48), 48, 48, -(80LL << 48)},

    // Q61.2 format (high precision)
    {3LL << 2, 2, 7LL << 2, 2, 2, 10LL << 2},
    {-(5LL << 2), 2, -(2LL << 2), 2, 2, -(7LL << 2)},

    // Q0.0 format (integers, same as integer tests but different category)
    {42, 0, 58, 0, 0, 100},
};
INSTANTIATE_TEST_SUITE_P(equal_precision, FixedAddTest,
                         ValuesIn(add_equal_precision));

/*
  Different Input Precisions

  Tests addition when the two operands have different precisions. Both are
  rescaled to max(augend_frac_bits, addend_frac_bits, output_frac_bits) before
  adding.
*/
const AddTestParams add_different_input_precision[] = {
    // Low precision augend, high precision addend, output matches addend
    {10, 0, 20LL << 32, 32, 32, (10LL << 32) + (20LL << 32)},
    {5LL << 16, 16, 10LL << 48, 48, 48, (5LL << 48) + (10LL << 48)},

    // High precision augend, low precision addend, output matches augend
    {10LL << 32, 32, 20, 0, 32, (10LL << 32) + (20LL << 32)},
    {10LL << 48, 48, 5LL << 16, 16, 48, (10LL << 48) + (5LL << 48)},

    // Both different, output matches neither
    {10LL << 16, 16, 20LL << 24, 24, 32, (10LL << 32) + (20LL << 32)},
    {5LL << 8, 8, 3LL << 16, 16, 24, (5LL << 24) + (3LL << 24)},

    // Negative values with mixed precisions
    {-(10LL << 32), 32, -20, 0, 32, -(10LL << 32) - (20LL << 32)},
    {-5, 0, -(3LL << 32), 32, 32, -(5LL << 32) - (3LL << 32)},
};
INSTANTIATE_TEST_SUITE_P(different_input_precision, FixedAddTest,
                         ValuesIn(add_different_input_precision));

/*
  Output Precision Conversion

  Tests where the output precision is different from both input precisions,
  requiring a final rescale after the addition.
*/
const AddTestParams add_output_precision_differs[] = {
    // Inputs at Q32, output at Q16 (downscale)
    {10LL << 32, 32, 20LL << 32, 32, 16, 30LL << 16},
    {(3LL << 32) + (1LL << 31), 32, (2LL << 32) + (1LL << 31), 32, 16,
     6LL << 16},  // 3.5 + 2.5 = 6.0 at different precision

    // Inputs at Q16, output at Q32 (upscale)
    {10LL << 16, 16, 20LL << 16, 16, 32, 30LL << 32},
    {5LL << 16, 16, 3LL << 16, 16, 48, 8LL << 48},

    // Inputs at different precisions, output at third precision
    {10LL << 16, 16, 20LL << 48, 48, 32, 30LL << 32},
    {5LL << 8, 8, 3LL << 40, 40, 24, 8LL << 24},

    // Output at Q0 (truncates fractional parts)

    // 10.5 + 20.5 = 31
    {(10LL << 32) + (1LL << 31), 32, (20LL << 32) + (1LL << 31), 32, 0, 31},

    // 5.75 + 3.25 = 9
    {(5LL << 32) + (3LL << 30), 32, (3LL << 32) + (1LL << 30), 32, 0, 9},
};
INSTANTIATE_TEST_SUITE_P(output_precision_differs, FixedAddTest,
                         ValuesIn(add_output_precision_differs));

/*
  All Sign Combinations

  Tests all four combinations of operand signs: pos+pos, pos+neg, neg+pos,
  neg+neg. The pos+neg and neg+pos cases effectively test subtraction behavior.
*/
const AddTestParams add_signs[] = {
    // Positive + Positive = Positive (larger)
    {100, 0, 50, 0, 0, 150},
    {10LL << 32, 32, 20LL << 32, 32, 32, 30LL << 32},

    // Positive + Negative result depends on magnitudes
    {100, 0, -50, 0, 0, 50},   // Result positive
    {50, 0, -100, 0, 0, -50},  // Result negative
    {100, 0, -100, 0, 0, 0},   // Result zero
    {(10LL << 32), 32, -(3LL << 32), 32, 32, 7LL << 32},

    // Negative + Positive result depends on magnitudes
    {-100, 0, 50, 0, 0, -50},  // Result negative
    {-50, 0, 100, 0, 0, 50},   // Result positive
    {-100, 0, 100, 0, 0, 0},   // Result zero
    {-(10LL << 32), 32, (3LL << 32), 32, 32, -(7LL << 32)},

    // Negative + Negative = Negative (larger magnitude)
    {-100, 0, -50, 0, 0, -150},
    {-(10LL << 32), 32, -(20LL << 32), 32, 32, -(30LL << 32)},
};
INSTANTIATE_TEST_SUITE_P(signs, FixedAddTest, ValuesIn(add_signs));

/*
  Positive Overflow Saturation

  Tests cases where two positive values add up to more than S64_MAX, requiring
  saturation to S64_MAX.
*/
const AddTestParams add_saturate_positive[] = {
    // Simple integer overflow
    {S64_MAX, 0, 1, 0, 0, S64_MAX},
    {S64_MAX, 0, 100, 0, 0, S64_MAX},
    {S64_MAX, 0, S64_MAX, 0, 0, S64_MAX},

    // Two large values that sum to overflow
    {(S64_MAX >> 1) + 1, 0, (S64_MAX >> 1) + 1, 0, 0, S64_MAX},
    {S64_MAX - 100, 0, 200, 0, 0, S64_MAX},

    // With fractional bits at same precision
    {S64_MAX, 32, 1LL << 32, 32, 32, S64_MAX},
    {(S64_MAX >> 1) + 1, 32, (S64_MAX >> 1) + 1, 32, 32, S64_MAX},

    // With different precisions (overflow after rescaling)
    {S64_MAX >> 16, 16, 1LL << 16, 16, 32, S64_MAX},
    {S64_MAX, 0, 1LL << 32, 32, 32, S64_MAX},

    // Overflow after upscaling to output precision
    {S64_MAX >> 1, 0, S64_MAX >> 1, 0, 1, S64_MAX},
};
INSTANTIATE_TEST_SUITE_P(saturate_positive, FixedAddTest,
                         ValuesIn(add_saturate_positive));

/*
  Negative Overflow Saturation

  Tests cases where two negative values add up to less than S64_MIN, requiring
  saturation to S64_MIN.
*/
const AddTestParams add_saturate_negative[] = {
    // Simple integer underflow
    {S64_MIN, 0, -1, 0, 0, S64_MIN},
    {S64_MIN, 0, -100, 0, 0, S64_MIN},
    {S64_MIN, 0, S64_MIN, 0, 0, S64_MIN},

    // Two large negative values that sum to underflow
    {(S64_MIN >> 1) - 1, 0, (S64_MIN >> 1) - 1, 0, 0, S64_MIN},
    {S64_MIN + 100, 0, -200, 0, 0, S64_MIN},

    // With fractional bits at same precision
    {S64_MIN, 32, -(1LL << 32), 32, 32, S64_MIN},
    {(S64_MIN >> 1) - 1, 32, (S64_MIN >> 1) - 1, 32, 32, S64_MIN},

    // With different precisions (underflow after rescaling)
    {S64_MIN >> 16, 16, -(1LL << 16), 16, 32, S64_MIN},
    {S64_MIN, 0, -(1LL << 32), 32, 32, S64_MIN},

    // Underflow after upscaling to output precision
    {(S64_MIN >> 1), 0, (S64_MIN >> 1), 0, 1, S64_MIN},
};
INSTANTIATE_TEST_SUITE_P(saturate_negative, FixedAddTest,
                         ValuesIn(add_saturate_negative));

/*
  Near Saturation Boundaries

  Tests values that are close to overflow/underflow but don't quite reach it,
  and values that are exactly at the boundary.
*/
const AddTestParams add_boundaries[] = {
    // Just under positive overflow
    {S64_MAX - 1, 0, 1, 0, 0, S64_MAX},         // Exactly at max
    {S64_MAX - 100, 0, 100, 0, 0, S64_MAX},     // Exactly at max
    {S64_MAX - 100, 0, 99, 0, 0, S64_MAX - 1},  // One below max

    // Just above negative underflow
    {S64_MIN + 1, 0, -1, 0, 0, S64_MIN},         // Exactly at min
    {S64_MIN + 100, 0, -100, 0, 0, S64_MIN},     // Exactly at min
    {S64_MIN + 100, 0, -99, 0, 0, S64_MIN + 1},  // One above min

    // Large values that just barely fit without overflow
    {S64_MAX >> 1, 0, S64_MAX >> 1, 0, 0, S64_MAX - 1},
    {(S64_MIN >> 1) + 1, 0, (S64_MIN >> 1) + 1, 0, 0, S64_MIN + 2},

    // With fractional bits
    {S64_MAX - (1LL << 32), 32, (1LL << 32) - 1, 32, 32, S64_MAX - 1},
    {S64_MIN + (1LL << 32), 32, -((1LL << 32) - 1), 32, 32, S64_MIN + 1},
};
INSTANTIATE_TEST_SUITE_P(boundaries, FixedAddTest, ValuesIn(add_boundaries));

/*
  Rounding Behavior

  Tests that when the result is rescaled to lower precision, the rounding is
  toward zero (truncation), not toward negative infinity (floor).
*/
const AddTestParams add_rounding[] = {
    // Positive results with fractional parts
    // 1.75 + 0.75 = 2.5, truncates to 2 at Q0
    {(7LL << 30), 32, (3LL << 30), 32, 0, 2},

    // 1.9375 + 0.9375 = 2.875, truncates to 2 at Q0
    {(1LL << 32) + (15LL << 28), 32, (1LL << 28) * 15, 32, 0, 2},

    // Negative results with fractional parts
    // -1.75 + -0.75 = -2.5, truncates to -2 at Q0 (toward zero, not -3)
    {-(7LL << 30), 32, -(3LL << 30), 32, 0, -2},

    // -1.9375 + -0.9375 = -2.875, truncates to -2 at Q0 (toward zero, not -3)
    {-((1LL << 32) + (15LL << 28)), 32, -(15LL << 28), 32, 0, -2},

    // Mixed signs: 11.25 - 5.5 = 5.75, truncates to 5
    {(11LL << 32) + (1LL << 30), 32, -((5LL << 32) + (1LL << 31)), 32, 0, 5},

    // Downscaling from Q32 to Q16
    // 3.999... + 2.0 = 5.999..., keeps precision at Q16
    {(3LL << 32) + ((1LL << 32) - 1), 32, 2LL << 32, 32, 16,
     (5LL << 16) + ((1LL << 16) - 1)},
};
INSTANTIATE_TEST_SUITE_P(rounding, FixedAddTest, ValuesIn(add_rounding));

/*
  S64 Boundary Values

  Tests involving S64_MAX and S64_MIN to ensure they're handled correctly in
  all contexts (as operands, after rescaling, etc.).
*/
const AddTestParams add_s64_boundaries[] = {
    // S64_MAX as operand
    {S64_MAX, 0, 0, 0, 0, S64_MAX},       // MAX + 0 = MAX
    {S64_MAX, 32, 0, 32, 32, S64_MAX},    // MAX + 0 at Q32 = MAX
    {S64_MAX, 0, -1, 0, 0, S64_MAX - 1},  // MAX + (-1) = MAX - 1
    {S64_MAX, 32, -(1LL << 32), 32, 32, S64_MAX - (1LL << 32)},

    // S64_MIN as operand
    {S64_MIN, 0, 0, 0, 0, S64_MIN},      // MIN + 0 = MIN
    {S64_MIN, 32, 0, 32, 32, S64_MIN},   // MIN + 0 at Q32 = MIN
    {S64_MIN, 0, 1, 0, 0, S64_MIN + 1},  // MIN + 1 = MIN + 1
    {S64_MIN, 32, 1LL << 32, 32, 32, S64_MIN + (1LL << 32)},

    // Both at boundaries with opposite signs (should not overflow)
    {S64_MAX, 0, S64_MIN, 0, 0, -1},  // MAX + MIN = -1
    {S64_MAX, 32, S64_MIN, 32, 32, -1},

    // Rescaling boundary values
    {S64_MAX, 0, 0, 0, 32, S64_MAX},  // Rescaling MAX saturates
    {S64_MIN, 0, 0, 0, 32, S64_MIN},  // Rescaling MIN saturates
};
INSTANTIATE_TEST_SUITE_P(s64_boundaries, FixedAddTest,
                         ValuesIn(add_s64_boundaries));

/*
  Practical Real-World Cases

  Addition operations that might appear in actual fixed-point calculations,
  with realistic precision combinations for common use cases.
*/
const AddTestParams add_realistic[] = {
    // Physics calculations (Q24.40 for position/velocity)
    {(10LL << 40), 40, (5LL << 40), 40, 40, (15LL << 40)},   // 10.0 + 5.0 m
    {(98LL << 38), 40, (5LL << 38), 40, 40, (103LL << 38)},  // 9.8 + 0.5 m/s^2

    // Graphics/normalized values (Q2.61 for [0,1] range)
    {1LL << 60, 61, 1LL << 59, 61, 61, 3LL << 59},  // 0.5 + 0.25 = 0.75
    {(1LL << 61) + (1LL << 60), 61, 1LL << 59, 61, 61,
     (1LL << 61) + (1LL << 60) + (1LL << 59)},  // 0.75 + 0.25 = 1.0

    // Frame time accumulation (Q32.32)
    // The results are summed this way to make sure they truncate the same as
    // the actual addition does.
    {(1LL << 32) / 60, 32, (1LL << 32) / 60, 32, 32,
     (1LL << 32) / 60 + (1LL << 32) / 60},  // Two 60fps frames
    {(1LL << 32) / 30, 32, (1LL << 32) / 30, 32, 32,
     (1LL << 32) / 30 + (1LL << 32) / 30},  // Two 30fps frames

    // Mixed precision realistic
    {100LL << 16, 16, 50, 0, 32, (150LL << 32)},     // Low + high precision
    {1000, 0, 500LL << 48, 48, 48, (1500LL << 48)},  // Integer + high precision
};
INSTANTIATE_TEST_SUITE_P(realistic, FixedAddTest, ValuesIn(add_realistic));

// ----------------------------------------------------------------------------
// Intermediate Saturation Tests
// ----------------------------------------------------------------------------

// This test suite does not use commutation.
struct FixedAddIntermediateSaturationTest : FixedAddTest {};

TEST_P(FixedAddIntermediateSaturationTest, expected_result) {
  const auto expected_result = GetParam().expected_result;

  const auto actual_result = curves_fixed_add(
      GetParam().augend, GetParam().augend_frac_bits, GetParam().addend,
      GetParam().addend_frac_bits, GetParam().output_frac_bits);

  ASSERT_EQ(expected_result, actual_result);
}

/*
  Category 1A: First Argument Overflows -> S64_MAX During Upscale

  Augend is a large positive value at low precision that saturates to S64_MAX
  when upscaled to max_frac_bits, then adds with a valid addend.
*/

// Augend overflows to S64_MAX, addend is small positive
const AddTestParams add_first_overflow_small_pos[] = {
    // augend: 2^53 at Q0 -> tries (2^53 << 32) -> saturates to S64_MAX at Q32
    // addend: 100 at Q32 -> stays 100 at Q32
    // sum: S64_MAX + 100 -> saturates to S64_MAX
    {S64_MAX >> 10, 0, 100LL << 32, 32, 32, S64_MAX},

    // Similar with different precisions
    {S64_MAX >> 15, 16, 50LL << 32, 32, 32, S64_MAX},
    {S64_MAX >> 7, 8, 200LL << 48, 48, 48, S64_MAX},

    //{S64_MAX >> 20, 0, 1000LL << 16, 16, 16, S64_MAX},
    {S64_MAX >> 15, 0, 1000LL << 16, 16, 16, S64_MAX},
};
INSTANTIATE_TEST_SUITE_P(first_overflow_small_pos,
                         FixedAddIntermediateSaturationTest,
                         ValuesIn(add_first_overflow_small_pos));

// Augend overflows to S64_MAX, addend is small negative
const AddTestParams add_first_overflow_small_neg[] = {
    // augend: saturates to S64_MAX at Q32
    // addend: -100 at Q32
    // sum: S64_MAX - 100 -> no saturation in add step
    {S64_MAX >> 10, 0, -(100LL << 32), 32, 32, S64_MAX - (100LL << 32)},

    // Similar with different values
    {S64_MAX >> 15, 16, -(50LL << 32), 32, 32, S64_MAX - (50LL << 32)},
    {S64_MAX >> 7, 8, -(200LL << 48), 48, 48, S64_MAX - (200LL << 48)},
};
INSTANTIATE_TEST_SUITE_P(first_overflow_small_neg,
                         FixedAddIntermediateSaturationTest,
                         ValuesIn(add_first_overflow_small_neg));

// Augend overflows to S64_MAX, addend is large negative (no saturation in add)
const AddTestParams add_first_overflow_large_neg[] = {
    // augend: saturates to S64_MAX at Q32
    // addend: S64_MIN at Q32 (already at max precision, no rescale)
    // sum: S64_MAX + S64_MIN = -1
    {S64_MAX >> 10, 0, S64_MIN, 32, 32, -1},

    // augend: saturates to S64_MAX at Q32
    // addend: -(2^62) at Q32
    // sum: S64_MAX - (2^62) = positive
    {S64_MAX >> 10, 0, -(1LL << 62), 32, 32, S64_MAX - (1LL << 62)},
};
INSTANTIATE_TEST_SUITE_P(first_overflow_large_neg,
                         FixedAddIntermediateSaturationTest,
                         ValuesIn(add_first_overflow_large_neg));

/*
  Category 1B: First Argument Underflows -> S64_MIN During Upscale

  Augend is a large negative value at low precision that saturates to S64_MIN
  when upscaled to max_frac_bits, then adds with a valid addend.
*/

// Augend underflows to S64_MIN, addend is small positive
const AddTestParams add_first_underflow_small_pos[] = {
    // augend: -(2^53) at Q0 -> saturates to S64_MIN at Q32
    // addend: 100 at Q32
    // sum: S64_MIN + 100 -> no saturation
    {S64_MIN >> 10, 0, 100LL << 32, 32, 32, S64_MIN + (100LL << 32)},

    {S64_MIN >> 15, 16, 50LL << 32, 32, 32, S64_MIN + (50LL << 32)},
    {S64_MIN >> 7, 8, 200LL << 48, 48, 48, S64_MIN + (200LL << 48)},
};
INSTANTIATE_TEST_SUITE_P(first_underflow_small_pos,
                         FixedAddIntermediateSaturationTest,
                         ValuesIn(add_first_underflow_small_pos));

// Augend underflows to S64_MIN, addend is large positive (no saturation)
const AddTestParams add_first_underflow_large_pos[] = {
    // augend: saturates to S64_MIN at Q32
    // addend: S64_MAX at Q32 (already at max precision)
    // sum: S64_MIN + S64_MAX = -1
    {S64_MIN >> 10, 0, S64_MAX, 32, 32, -1},

    // augend: saturates to S64_MIN at Q32
    // addend: 2^62 at Q32
    // sum: S64_MIN + 2^62 = negative
    {S64_MIN >> 10, 0, 1LL << 62, 32, 32, S64_MIN + (1LL << 62)},
};
INSTANTIATE_TEST_SUITE_P(first_underflow_large_pos,
                         FixedAddIntermediateSaturationTest,
                         ValuesIn(add_first_underflow_large_pos));

// Augend underflows to S64_MIN, addend is small negative (saturates in add)
const AddTestParams add_first_underflow_small_neg[] = {
    // augend: saturates to S64_MIN at Q32
    // addend: -100 at Q32
    // sum: S64_MIN - 100 -> saturates to S64_MIN
    {S64_MIN >> 10, 0, -(100LL << 32), 32, 32, S64_MIN},

    {S64_MIN >> 15, 16, -(50LL << 32), 32, 32, S64_MIN},
    {S64_MIN >> 7, 8, -(200LL << 48), 48, 48, S64_MIN},
};
INSTANTIATE_TEST_SUITE_P(first_underflow_small_neg,
                         FixedAddIntermediateSaturationTest,
                         ValuesIn(add_first_underflow_small_neg));

/*
  Category 2A: Second Argument Overflows -> S64_MAX During Upscale

  Addend saturates to S64_MAX when upscaled, then adds with valid augend.
*/

// Addend overflows to S64_MAX, augend is small positive
const AddTestParams add_second_overflow_small_pos[] = {
    // augend: 100 at Q32 (already at max precision)
    // addend: 2^53 at Q0 -> saturates to S64_MAX at Q32
    // sum: 100 + S64_MAX -> saturates to S64_MAX
    {100LL << 32, 32, S64_MAX >> 10, 0, 32, S64_MAX},

    {50LL << 32, 32, S64_MAX >> 15, 16, 32, S64_MAX},
    {200LL << 48, 48, S64_MAX >> 7, 8, 48, S64_MAX},
};
INSTANTIATE_TEST_SUITE_P(second_overflow_small_pos,
                         FixedAddIntermediateSaturationTest,
                         ValuesIn(add_second_overflow_small_pos));

// Addend overflows to S64_MAX, augend is small negative
const AddTestParams add_second_overflow_small_neg[] = {
    // augend: -100 at Q32
    // addend: saturates to S64_MAX at Q32
    // sum: -100 + S64_MAX = S64_MAX - 100
    {-(100LL << 32), 32, S64_MAX >> 10, 0, 32, S64_MAX - (100LL << 32)},

    {-(50LL << 32), 32, S64_MAX >> 15, 16, 32, S64_MAX - (50LL << 32)},
};
INSTANTIATE_TEST_SUITE_P(second_overflow_small_neg,
                         FixedAddIntermediateSaturationTest,
                         ValuesIn(add_second_overflow_small_neg));

// Addend overflows to S64_MAX, augend is large negative
const AddTestParams add_second_overflow_large_neg[] = {
    // augend: S64_MIN at Q32
    // addend: saturates to S64_MAX at Q32
    // sum: S64_MIN + S64_MAX = -1
    {S64_MIN, 32, S64_MAX >> 10, 0, 32, -1},

    // augend: -(2^62) at Q32
    // addend: saturates to S64_MAX at Q32
    // sum: -(2^62) + S64_MAX = positive
    {-(1LL << 62), 32, S64_MAX >> 10, 0, 32, S64_MAX - (1LL << 62)},
};
INSTANTIATE_TEST_SUITE_P(second_overflow_large_neg,
                         FixedAddIntermediateSaturationTest,
                         ValuesIn(add_second_overflow_large_neg));

/*
  Category 2B: Second Argument Underflows -> S64_MIN During Upscale

  Addend saturates to S64_MIN when upscaled, then adds with valid augend.
*/

// Addend underflows to S64_MIN, augend is small positive
const AddTestParams add_second_underflow_small_pos[] = {
    // augend: 100 at Q32
    // addend: -(2^53) at Q0 -> saturates to S64_MIN at Q32
    // sum: 100 + S64_MIN = S64_MIN + 100
    {100LL << 32, 32, S64_MIN >> 10, 0, 32, S64_MIN + (100LL << 32)},

    {50LL << 32, 32, S64_MIN >> 15, 16, 32, S64_MIN + (50LL << 32)},
};
INSTANTIATE_TEST_SUITE_P(second_underflow_small_pos,
                         FixedAddIntermediateSaturationTest,
                         ValuesIn(add_second_underflow_small_pos));

// Addend underflows to S64_MIN, augend is large positive
const AddTestParams add_second_underflow_large_pos[] = {
    // augend: S64_MAX at Q32
    // addend: saturates to S64_MIN at Q32
    // sum: S64_MAX + S64_MIN = -1
    {S64_MAX, 32, S64_MIN >> 10, 0, 32, -1},

    // augend: 2^62 at Q32
    // addend: saturates to S64_MIN at Q32
    // sum: 2^62 + S64_MIN = negative
    {1LL << 62, 32, S64_MIN >> 10, 0, 32, S64_MIN + (1LL << 62)},
};
INSTANTIATE_TEST_SUITE_P(second_underflow_large_pos,
                         FixedAddIntermediateSaturationTest,
                         ValuesIn(add_second_underflow_large_pos));

// Addend underflows to S64_MIN, augend is small negative (saturates in add)
const AddTestParams add_second_underflow_small_neg[] = {
    // augend: -100 at Q32
    // addend: saturates to S64_MIN at Q32
    // sum: -100 + S64_MIN -> saturates to S64_MIN
    {-(100LL << 32), 32, S64_MIN >> 10, 0, 32, S64_MIN},

    {-(50LL << 32), 32, S64_MIN >> 15, 16, 32, S64_MIN},
};
INSTANTIATE_TEST_SUITE_P(second_underflow_small_neg,
                         FixedAddIntermediateSaturationTest,
                         ValuesIn(add_second_underflow_small_neg));

/*
  Category 3: Both Arguments Saturate During Upscale
*/

// Both overflow to S64_MAX
const AddTestParams add_both_overflow[] = {
    // Both at Q0, both overflow to S64_MAX at Q32
    // sum: S64_MAX + S64_MAX -> saturates to S64_MAX
    {S64_MAX >> 10, 0, S64_MAX >> 10, 0, 32, S64_MAX},

    // Different low precisions, both overflow
    {S64_MAX >> 15, 16, S64_MAX >> 7, 8, 32, S64_MAX},
    {S64_MAX >> 20, 0, S64_MAX >> 10, 10, 48, S64_MAX},
};
INSTANTIATE_TEST_SUITE_P(both_overflow, FixedAddIntermediateSaturationTest,
                         ValuesIn(add_both_overflow));

// Both underflow to S64_MIN
const AddTestParams add_both_underflow[] = {
    // Both at Q0, both underflow to S64_MIN at Q32
    // sum: S64_MIN + S64_MIN -> saturates to S64_MIN
    {S64_MIN >> 10, 0, S64_MIN >> 10, 0, 32, S64_MIN},

    {S64_MIN >> 15, 16, S64_MIN >> 7, 8, 32, S64_MIN},
    {S64_MIN >> 20, 0, S64_MIN >> 10, 10, 48, S64_MIN},
};
INSTANTIATE_TEST_SUITE_P(both_underflow, FixedAddIntermediateSaturationTest,
                         ValuesIn(add_both_underflow));

// One overflows, one underflows
const AddTestParams add_opposite_saturation[] = {
    // augend: saturates to S64_MAX at Q32
    // addend: saturates to S64_MIN at Q32
    // sum: S64_MAX + S64_MIN = -1
    {S64_MAX >> 10, 0, S64_MIN >> 10, 0, 32, -1},

    // Flipped order
    {S64_MIN >> 10, 0, S64_MAX >> 10, 0, 32, -1},

    // Different precisions
    {S64_MAX >> 15, 16, S64_MIN >> 7, 8, 32, -1},
    {S64_MIN >> 20, 10, S64_MAX >> 10, 20, 48, -1},
};
INSTANTIATE_TEST_SUITE_P(opposite_saturation,
                         FixedAddIntermediateSaturationTest,
                         ValuesIn(add_opposite_saturation));

/*
  Category 4A: Intermediate Saturation + Output Upscale Saturation

  Args saturate during upscale to max, then output needs even higher precision
  causing another saturation.
*/

// First arg saturates, then output upscale also saturates
const AddTestParams add_intermediate_saturation_only[] = {
    // augend: saturates to S64_MAX at Q32
    // addend: 100 at Q32
    // sum: S64_MAX + 100 -> S64_MAX at Q32
    // output: S64_MAX at Q32 -> S64_MAX at Q33 (left shift by 1 saturates)
    {S64_MAX >> 10, 0, 100LL << 32, 32, 33, S64_MAX},

    // Similar with underflow
    {S64_MIN >> 10, 0, -(100LL << 32), 32, 33, S64_MIN},

    // Both saturate to same sign, then output upscale saturates
    {S64_MAX >> 10, 0, S64_MAX >> 10, 0, 32,
     S64_MAX},  // Already at max, but check
    {S64_MAX >> 10, 0, S64_MAX >> 15, 16, 33, S64_MAX},
};
INSTANTIATE_TEST_SUITE_P(intermediate_then_output_upscale,
                         FixedAddIntermediateSaturationTest,
                         ValuesIn(add_intermediate_saturation_only));

/*
  Category 4B: Intermediate Saturation + Output Downscale

  Args saturate during upscale to max, then output has lower precision
  requiring downscale (not upscale).
*/

// Intermediate saturation, then downscale for output
const AddTestParams add_intermediate_then_downscale[] = {
    // augend: saturates to S64_MAX at Q32
    // addend: 100 at Q32
    // sum: S64_MAX at Q32 (saturated in add step)
    // output: S64_MAX
    {S64_MAX >> 10, 0, 100LL << 32, 32, 16, S64_MAX},  // Stays saturated

    // Similar with underflow
    {S64_MIN >> 10, 0, -(100LL << 32), 32, 16, S64_MIN},  // Stays saturated

    // Both saturate, then downscale
    {S64_MAX >> 10, 0, S64_MAX >> 10, 0, 16, S64_MAX},  // Stays saturated
    {S64_MIN >> 10, 0, S64_MIN >> 10, 0, 16, S64_MIN},  // Stays saturated

    // Opposite saturation -> -1, then downscale
    {S64_MAX >> 10, 0, S64_MIN >> 10, 0, 16, -1},
};
INSTANTIATE_TEST_SUITE_P(intermediate_then_downscale,
                         FixedAddIntermediateSaturationTest,
                         ValuesIn(add_intermediate_then_downscale));

}  // namespace
}  // namespace curves
