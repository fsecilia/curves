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
// curves_fixed_multiply()
// ----------------------------------------------------------------------------

struct MultiplyTestParams {
  s64 multiplicand;
  unsigned int multiplicand_frac_bits;
  s64 multiplier;
  unsigned int multiplier_frac_bits;
  unsigned int output_frac_bits;
  s64 expected_result;

  friend auto operator<<(std::ostream& out, const MultiplyTestParams& src)
      -> std::ostream& {
    return out << "{" << src.multiplicand << ", " << src.multiplicand_frac_bits
               << ", " << src.multiplier << ", " << src.multiplier_frac_bits
               << ", " << src.output_frac_bits << ", " << src.expected_result
               << "}";
  }
};

struct FixedMultiplyTest : TestWithParam<MultiplyTestParams> {};

TEST_P(FixedMultiplyTest, expected_result) {
  const auto expected_result = GetParam().expected_result;

  const auto actual_result = curves_fixed_multiply(
      GetParam().multiplicand, GetParam().multiplicand_frac_bits,
      GetParam().multiplier, GetParam().multiplier_frac_bits,
      GetParam().output_frac_bits);

  ASSERT_EQ(expected_result, actual_result);
}

/*
  This switches both multiplier and multiplicand and their frac bits. Because
  multiplication is commutative, we can reduce the number of test cases by only
  including combinations, rather than permutations.
*/
TEST_P(FixedMultiplyTest, multiplication_is_commutative) {
  const auto expected_result = GetParam().expected_result;

  const auto actual_result = curves_fixed_multiply(
      GetParam().multiplier, GetParam().multiplier_frac_bits,
      GetParam().multiplicand, GetParam().multiplicand_frac_bits,
      GetParam().output_frac_bits);

  ASSERT_EQ(expected_result, actual_result);
}

/*
  This switches only the frac bits, because they are summed, so we can reduce
  the number of test cases even further.
*/
TEST_P(FixedMultiplyTest, frac_bits_order_doesnt_matter) {
  const auto expected_result = GetParam().expected_result;

  const auto actual_result = curves_fixed_multiply(
      GetParam().multiplicand, GetParam().multiplier_frac_bits,
      GetParam().multiplier, GetParam().multiplicand_frac_bits,
      GetParam().output_frac_bits);

  ASSERT_EQ(expected_result, actual_result);
}

// Zero multiplied by anything yields zero, regardless of precision.
const MultiplyTestParams multiply_zero[] = {
    {0, 0, 0, 0, 0, 0},       // Zero precision
    {0, 32, 0, 32, 32, 0},    // Mid precision
    {0, 62, 5, 62, 62, 0},    // High precision, non-zero multiplier
    {100, 32, 0, 32, 32, 0},  // Non-zero multiplicand
};
INSTANTIATE_TEST_SUITE_P(zero, FixedMultiplyTest, ValuesIn(multiply_zero));

// Multiplying by 1 should preserve the value (with rescaling).
const MultiplyTestParams multiply_identity[] = {
    // At zero precision: 2 * 1 = 2
    {2, 0, 1, 0, 0, 2},

    // At 32 bits: 5 * 1.0 = 5.0
    {5LL << 32, 32, 1LL << 32, 32, 32, 5LL << 32},

    // Different input precisions, same output precision
    {3LL << 16, 16, 1LL << 32, 32, 32, 3LL << 32},
};
INSTANTIATE_TEST_SUITE_P(identity, FixedMultiplyTest,
                         ValuesIn(multiply_identity));

// Simple integer multiplication (frac_bits = 0 for all).
const MultiplyTestParams multiply_integers[] = {
    {2, 0, 3, 0, 0, 6},   {5, 0, 7, 0, 0, 35},   {10, 0, 10, 0, 0, 100},
    {-2, 0, 3, 0, 0, -6}, {-5, 0, -7, 0, 0, 35},
};
INSTANTIATE_TEST_SUITE_P(integers, FixedMultiplyTest,
                         ValuesIn(multiply_integers));

// Basic fractional multiplication with simple, verifiable values.
const MultiplyTestParams multiply_simple_fractions[] = {
    // 2.0 * 3.0 = 6.0, all at q31.32
    {2LL << 32, 32, 3LL << 32, 32, 32, 6LL << 32},

    // 2.5 * 2.0 = 5.0, at q1.31 (2.5 = 5/2, so (5 << 31) / 2 = 2.5)
    {5LL << 30, 31, 2LL << 31, 31, 31, 5LL << 31},

    // 1.5 * 2.0 = 3.0, at q15.48
    {(3LL << 47), 48, (2LL << 48), 48, 48, 3LL << 48},

    // Negative: -2.0 * 3.0 = -6.0
    {-(2LL << 32), 32, 3LL << 32, 32, 32, -(6LL << 32)},
};
INSTANTIATE_TEST_SUITE_P(simple_fractions, FixedMultiplyTest,
                         ValuesIn(multiply_simple_fractions));

// Multiplying values with different input and output precisions.
const MultiplyTestParams multiply_precision_conversion[] = {
    // 2.0 (q31.32) * 3.0 (q15.48) = 6.0 (q31.32)
    // Input sum: 32 + 48 = 80 fractional bits
    // Output: 32 fractional bits (right shift by 48)
    {2LL << 32, 32, 3LL << 48, 48, 32, 6LL << 32},

    // 5.0 (q47.16) * 2.0 (q47.16) = 10.0 (q31.32)
    // Input sum: 16 + 16 = 32 fractional bits
    // Output: 32 fractional bits (no shift needed)
    {5LL << 16, 16, 2LL << 16, 16, 32, 10LL << 32},

    // 4.0 (q15.48) * 2.0 (q31.32) = 8.0 (q47.16)
    // Input sum: 48 + 32 = 80 fractional bits
    // Output: 16 fractional bits (right shift by 64)
    {4LL << 48, 48, 2LL << 32, 32, 16, 8LL << 16},

    // Increase precision: 3 (q63.0) * 2 (q63.0) = 6.0 (q31.32)
    // Input sum: 0 + 0 = 0 fractional bits
    // Output: 32 fractional bits (left shift by 32)
    {3, 0, 2, 0, 32, 6LL << 32},
};
INSTANTIATE_TEST_SUITE_P(precision_conversion, FixedMultiplyTest,
                         ValuesIn(multiply_precision_conversion));

// Verify round-to-zero behavior when precision is reduced.
const MultiplyTestParams multiply_rounding[] = {
    // Positive: 1.5 * 1.5 = 2.25, truncates to 2.0 (not 2.5 or 3.0)
    // At q1.61: 1.5 = 3 << 60, so 1.5 * 1.5 = (3 << 60) * (3 << 60) = 9 << 120
    // Intermediate in q2.122: 9 << 120
    // After rescale to q62.0 (shift right by 122): should be 2
    {3LL << 60, 61, 3LL << 60, 61, 0, 2},

    // Negative: -1.5 * 1.5 = -2.25, truncates to -2.0 (toward zero)
    {-(3LL << 60), 61, 3LL << 60, 61, 0, -2},

    // Smaller fractional part: 2.25 * 1.0 = 2.25, output as integer = 2
    // 2.25 in q30.32 is (9 << 32) / 4 = (9 << 30)
    {9LL << 30, 32, 1LL << 32, 32, 0, 2},

    // Just under a boundary: 2.999... rounds to 2
    // Use (3 << 32) - 1 to represent 2.999... in q31.32
    {(3LL << 32) - 1, 32, 1LL << 32, 32, 0, 2},
};
INSTANTIATE_TEST_SUITE_P(rounding, FixedMultiplyTest,
                         ValuesIn(multiply_rounding));

// Verify correct sign handling for all input sign combinations.
const MultiplyTestParams multiply_signs[] = {
    // Positive * Positive = Positive
    {3LL << 32, 32, 2LL << 32, 32, 32, 6LL << 32},

    // Positive * Negative = Negative
    {3LL << 32, 32, -(2LL << 32), 32, 32, -(6LL << 32)},

    // Negative * Positive = Negative (will be tested via commutativity)
    // (This case is covered by commutativity test from positive * negative)

    // Negative * Negative = Positive
    {-(3LL << 32), 32, -(2LL << 32), 32, 32, 6LL << 32},

    // Edge case: multiplying by -1 should negate
    {5LL << 32, 32, -(1LL << 32), 32, 32, -(5LL << 32)},
    {-(5LL << 32), 32, -(1LL << 32), 32, 32, 5LL << 32},
};
INSTANTIATE_TEST_SUITE_P(signs, FixedMultiplyTest, ValuesIn(multiply_signs));

// Verify saturation when the result is too large for s64.
const MultiplyTestParams multiply_saturation[] = {
    // Positive overflow: Large positive values that exceed S64_MAX
    // S64_MAX is about 9.2e18. If we multiply two values near sqrt(S64_MAX)
    // which is about 3e9, we'll overflow.
    // Use S64_MAX >> 10 for each operand, which when multiplied gives a
    // value larger than S64_MAX even after rescaling.
    {S64_MAX >> 10, 32, S64_MAX >> 10, 32, 32, S64_MAX},

    // Even more extreme: multiply maximum values at low precision
    {S64_MAX, 0, S64_MAX, 0, 0, S64_MAX},

    // Negative overflow: Large negative values that exceed S64_MIN
    // Similar logic but with negative values
    {S64_MIN >> 10, 32, S64_MAX >> 10, 32, 32, S64_MIN},

    // Negative * Negative overflowing to positive
    {S64_MIN >> 10, 32, S64_MIN >> 10, 32, 32, S64_MAX},

    // Maximum negative value
    {S64_MIN, 0, S64_MAX, 0, 0, S64_MIN},
    {S64_MIN, 0, S64_MIN, 0, 0, S64_MAX},
};
INSTANTIATE_TEST_SUITE_P(saturation, FixedMultiplyTest,
                         ValuesIn(multiply_saturation));

// Large values that fit correctly without saturating.
const MultiplyTestParams multiply_boundaries[] = {
    // Values that are large but whose product fits in s64
    // For q31.32: max safe value is roughly sqrt(S64_MAX >> 32)
    // That's about sqrt(2^31) = 2^15.5 ~= 46340
    {46340LL << 32, 32, 46340LL << 32, 32, 32, (46340LL * 46340LL) << 32},

    // At integer precision: smaller values
    {1000000, 0, 1000000, 0, 0, 1000000000000LL},

    // Negative boundaries
    {-46340LL << 32, 32, 46340LL << 32, 32, 32, -(46340LL * 46340LL) << 32},

    // One value at maximum, other small
    {S64_MAX, 0, 1, 0, 0, S64_MAX},
    {S64_MIN, 0, 1, 0, 0, S64_MIN},
};
INSTANTIATE_TEST_SUITE_P(boundaries, FixedMultiplyTest,
                         ValuesIn(multiply_boundaries));

}  // namespace
}  // namespace curves
