// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include "fixed.hpp"
#include "fixed_io.hpp"
#include <curves/test.hpp>

namespace curves {
namespace {

// ----------------------------------------------------------------------------
// Integer Conversions Tests
// ----------------------------------------------------------------------------

// Symmetric
// ----------------------------------------------------------------------------

struct SymmetricIntegerConversionParam {
  int64_t integer_value;
  unsigned int frac_bits;
  s64 fixed_value;

  friend auto operator<<(std::ostream& out,
                         const SymmetricIntegerConversionParam& src)
      -> std::ostream& {
    return out << "{" << src.integer_value << ", " << src.frac_bits << ", "
               << src.fixed_value << "}";
  }
};

/*
  These tests use values that don't need to round the fixed value, so they are
  the same in either direction, int->fixed or fixed->int.
*/
struct FixedTestSymmetricIntegerConversion
    : public TestWithParam<SymmetricIntegerConversionParam> {};

TEST_P(FixedTestSymmetricIntegerConversion, to_fixed) {
  const auto param = GetParam();

  const auto expected = param.fixed_value;
  const auto actual =
      curves_fixed_from_integer(param.integer_value, param.frac_bits);

  ASSERT_EQ(expected, actual);
}

TEST_P(FixedTestSymmetricIntegerConversion, to_integer) {
  const auto param = GetParam();

  const auto expected = param.integer_value;
  const auto actual =
      curves_fixed_to_integer(param.fixed_value, param.frac_bits);

  ASSERT_EQ(expected, actual);
}

const SymmetricIntegerConversionParam sym_int_near_zero[] = {
    // -2
    {-2, 1, -2LL << 1},
    {-2, 32, -2LL << 32},
    {-2, 61, -2LL << 61},

    // -1
    {-1, 1, -1LL << 1},
    {-1, 32, -1LL << 32},
    {-1, 62, -1LL << 62},

    // zero
    {0, 1, 0},
    {0, 32, 0},
    {0, 63, 0},

    // 1
    {1, 1, 1LL << 1},
    {1, 32, 1LL << 32},
    {1, 62, 1LL << 62},

    // 2
    {2, 1, 2LL << 1},
    {2, 32, 2LL << 32},
    {2, 61, 2LL << 61},
};
INSTANTIATE_TEST_SUITE_P(near_zero, FixedTestSymmetricIntegerConversion,
                         ValuesIn(sym_int_near_zero));

const SymmetricIntegerConversionParam sym_int_negative_boundaries[] = {
    // end of q15.48 range
    {-1LL << 15, 1, (-1LL << 15) << 1},
    {-1LL << 15, 24, (-1LL << 15) << 24},
    {-1LL << 15, 48, (-1LL << 15) << 48},

    // end of q31.32 range
    {-1LL << 31, 1, (-1LL << 31) << 1},
    {-1LL << 31, 16, (-1LL << 31) << 16},
    {-1LL << 31, 32, (-1LL << 31) << 32},

    // end of q47.16 range
    {-1LL << 47, 1, (-1LL << 47) << 1},
    {-1LL << 47, 8, (-1LL << 47) << 8},
    {-1LL << 47, 16, (-1LL << 47) << 16},

    // end of q62.1 range
    {-1ll << 62, 1, (-1ll << 62) << 1},

    // end of q63.0 range (S64_MIN)
    {S64_MIN, 0, S64_MIN},
};
INSTANTIATE_TEST_SUITE_P(negative_boundaries,
                         FixedTestSymmetricIntegerConversion,
                         ValuesIn(sym_int_negative_boundaries));

const SymmetricIntegerConversionParam sym_int_positive_boundaries[] = {
    // end of q15.48 range
    {(1LL << 15) - 1, 1, ((1LL << 15) - 1) << 1},
    {(1LL << 15) - 1, 24, ((1LL << 15) - 1) << 24},
    {(1LL << 15) - 1, 48, ((1LL << 15) - 1) << 48},

    // end of q31.32 range
    {(1LL << 31) - 1, 1, ((1LL << 31) - 1) << 1},
    {(1LL << 31) - 1, 16, ((1LL << 31) - 1) << 16},
    {(1LL << 31) - 1, 32, ((1LL << 31) - 1) << 32},

    // end of q47.16 range
    {(1LL << 47) - 1, 1, ((1LL << 47) - 1) << 1},
    {(1LL << 47) - 1, 8, ((1LL << 47) - 1) << 8},
    {(1LL << 47) - 1, 16, ((1LL << 47) - 1) << 16},

    // end of q62.1 range
    {(1ll << 62) - 1, 1, ((1ll << 62) - 1) << 1},

    // end of q63.0 range (S64_MAX)
    {S64_MAX, 0, S64_MAX},
};
INSTANTIATE_TEST_SUITE_P(positive_boundaries,
                         FixedTestSymmetricIntegerConversion,
                         ValuesIn(sym_int_positive_boundaries));

// Rounding
// ----------------------------------------------------------------------------

struct RoundingIntegerConversionParam {
  s64 fixed_value;
  unsigned int frac_bits;
  int64_t integer_value;

  friend auto operator<<(std::ostream& out,
                         const RoundingIntegerConversionParam& src)
      -> std::ostream& {
    return out << "{" << src.integer_value << ", " << src.frac_bits << ", "
               << src.fixed_value << "}";
  }
};

/*
  These test that fixed->integer conversions always round-to-zero, rather than
  the default integer behavior to round towards negative infinity.

  This conversion is implemented in terms of curves_fixed_rescale_s64, which
  has already been tested extensively. This test just checks a few specific
  rounding cases with high precision.
*/
struct FixedTestRoundingIntegerConversion
    : public TestWithParam<RoundingIntegerConversionParam> {};

TEST_P(FixedTestRoundingIntegerConversion, conversion_to_integer_truncates) {
  const auto param = GetParam();

  const auto expected = param.integer_value;
  const auto actual =
      curves_fixed_to_integer(param.fixed_value, param.frac_bits);

  ASSERT_EQ(expected, actual);
}

const RoundingIntegerConversionParam round_ints_negative[] = {
    {-4611686018427387904LL, 61, -2},  // = -2, floors to -2, truncates to -2
    {-4611686018427387903LL, 61, -1},  // < -2, floors to -2, truncates to -1
    {-3458764513820540928LL, 61, -1},  // = -1.5, floors to -2, truncates to -1
    {-3458764513820540927LL, 61, -1},  // < -1.5, floors to -2, truncates to -1
    {-2305843009213693952LL, 61, -1},  // = -1, floors to -1, truncates to -1
    {-2305843009213693951LL, 61, 0},   // < -1, floors to -1, truncates to 0
    {-1152921504606846976LL, 61, 0},   // = -0.5, floors to -1, truncates to 0
    {-1152921504606846975LL, 61, 0},   // < -0.5, floors -1, truncates to 0
};
INSTANTIATE_TEST_SUITE_P(negative, FixedTestRoundingIntegerConversion,
                         ValuesIn(round_ints_negative));

const RoundingIntegerConversionParam round_ints_near_zero[] = {
    {1LL, 61, 0},   // > 0, floors to 0, truncates to 0
    {0LL, 61, 0},   // = 0, floors to 0, truncates to 0
    {-1LL, 61, 0},  // < 0, floors to 0, truncates to 0
};
INSTANTIATE_TEST_SUITE_P(near_zero, FixedTestRoundingIntegerConversion,
                         ValuesIn(round_ints_near_zero));

const RoundingIntegerConversionParam round_ints_positive[] = {
    {1152921504606846975LL, 61, 0},  // < 0.5, floors to 0, truncates to 0
    {1152921504606846976LL, 61, 0},  // = 0.5, floors to 0, truncates to 0
    {2305843009213693951LL, 61, 0},  // < 1, floors to 0, truncates to 0
    {2305843009213693952LL, 61, 1},  // = 1, floors to 1, truncates to 1
    {3458764513820540927LL, 61, 1},  // < 1.5, floors to 1, truncates to 1
    {3458764513820540928LL, 61, 1},  // = 1.5, floors to 1, truncates to 1
    {4611686018427387903LL, 61, 1},  // < 2, floors to 1, truncates to 1
    {4611686018427387904LL, 61, 2},  // = 2, floors to 2, truncates to 2
};
INSTANTIATE_TEST_SUITE_P(positive, FixedTestRoundingIntegerConversion,
                         ValuesIn(round_ints_positive));

// frac_bits = 0: Special case, no rounding.
const RoundingIntegerConversionParam round_ints_edge_case_0[] = {
    {S64_MIN, 0, S64_MIN},
    {S64_MIN + 1, 0, S64_MIN + 1},
    {S64_MAX - 1, 0, S64_MAX - 1},
    {S64_MAX, 0, S64_MAX},
};
INSTANTIATE_TEST_SUITE_P(edge_case_0, FixedTestRoundingIntegerConversion,
                         ValuesIn(round_ints_edge_case_0));

// frac_bits = 1: Lowest precision that isn't just integers.
const RoundingIntegerConversionParam round_ints_edge_case_1[] = {
    {S64_MIN, 1, S64_MIN >> 1},           {S64_MIN + 1, 1, (S64_MIN >> 1) + 1},
    {S64_MAX - 2, 1, (S64_MAX >> 1) - 1}, {S64_MAX - 1, 1, S64_MAX >> 1},
    {S64_MAX, 1, S64_MAX >> 1},
};
INSTANTIATE_TEST_SUITE_P(edge_case_1, FixedTestRoundingIntegerConversion,
                         ValuesIn(round_ints_edge_case_1));

// frac_bits = 32: Typical precision
const RoundingIntegerConversionParam round_ints_edge_case_32[] = {
    {S64_MIN, 32, S64_MIN >> 32},
    {S64_MIN + 1, 32, (S64_MIN >> 32) + 1},
    {S64_MAX - (1LL << 32), 32, (S64_MAX >> 32) - 1},
    {S64_MAX - (1LL << 32) + 1, 32, (S64_MAX >> 32)},
    {S64_MAX, 32, (S64_MAX >> 32)},
};
INSTANTIATE_TEST_SUITE_P(edge_case_32, FixedTestRoundingIntegerConversion,
                         ValuesIn(round_ints_edge_case_32));

// frac_bits = 61: Highest precision that doesn't hit range boundary.
const RoundingIntegerConversionParam round_ints_edge_case_61[] = {
    {S64_MIN, 61, -4},
    {S64_MIN + 1, 61, -3},
    {S64_MAX - (1LL << 61), 61, 2},
    {S64_MAX - (1LL << 61) + 1, 61, 3},
    {S64_MAX, 61, 3},
};
INSTANTIATE_TEST_SUITE_P(edge_case_61, FixedTestRoundingIntegerConversion,
                         ValuesIn(round_ints_edge_case_61));

// frac_bits = 62: Maximum precision
const RoundingIntegerConversionParam round_ints_edge_case_62[] = {
    {S64_MIN, 62, -2},
    {S64_MIN + 1, 62, -1},
    {S64_MAX - (1LL << 62), 62, 0},
    {S64_MAX - (1LL << 62) + 1, 62, 1},
    {S64_MAX, 62, 1},
};
INSTANTIATE_TEST_SUITE_P(edge_case_62, FixedTestRoundingIntegerConversion,
                         ValuesIn(round_ints_edge_case_62));

// ----------------------------------------------------------------------------
// Double Conversions Tests
// ----------------------------------------------------------------------------

// Double -> Fixed
// ----------------------------------------------------------------------------

struct FixedFromDoubleParam {
  double double_value;
  s64 fixed_value;
  unsigned int frac_bits;

  friend auto operator<<(std::ostream& out, const FixedFromDoubleParam& src)
      -> std::ostream& {
    return out << "{" << src.double_value << ", " << src.fixed_value << ", "
               << src.frac_bits << "}";
  }
};

// Tests that doubles round to nearest during conversion to fixed-point.
struct FixedConversionTestFixedFromDouble
    : TestWithParam<FixedFromDoubleParam> {};

TEST_P(FixedConversionTestFixedFromDouble, from_double) {
  const auto param = GetParam();
  const auto actual =
      curves_fixed_from_double(param.double_value, param.frac_bits);
  ASSERT_EQ(param.fixed_value, actual);
}

// frac_bits = 0, no scaling occurs, so this is simple double->integer rounding.
const FixedFromDoubleParam from_double_frac_bits_0[] = {
    {-123.45, -123, 0},
    {-0.9, 0, 0},
    {0.9, 0, 0},
    {123.45, 123, 0},

    /*
      Min and max representable values.

      Ideally, we'd test against S64_MAX, but it is a 63-bit number. A double
      only has 53 bits of precision, so S64_MAX can't be stored precisely in a
      double. If we were to try to round trip it, the runtime would have to
      pick the closest representable double, which in this case, causes it to
      round up to 2^64. The value in the double is then larger than S64_MAX.
      Converting an out of range double to an integer is undefined behavior. In
      this specific case, on x64, converting back just happens to give the
      value S64_MIN. That is about as different from the value we started with
      as could be, so the test fails.

      Instead, we use the largest round-trippable integer, which is:
        (2^63 - 1) - (2^10 - 1) = 2^63 - 2^10 = S64_MAX - 1023

      S64_MIN is representable, so we use it directly.
    */
    {static_cast<double>(S64_MIN), S64_MIN, 0},
    {static_cast<double>(S64_MAX - 1023), S64_MAX - 1023, 0},
};
INSTANTIATE_TEST_SUITE_P(frac_bits_0, FixedConversionTestFixedFromDouble,
                         ValuesIn(from_double_frac_bits_0));

// frac_bits = 32
const FixedFromDoubleParam from_double_frac_bits_32[] = {
    {-123.45, -530213712691, 32},
    {-0.9, -3865470566, 32},
    {0.9, 3865470566, 32},
    {123.45, 530213712691, 32},

    /*
      With 32 fractional bits, the smallest bit represents 1/2^32. 2^-33 is
      half of that, so the fixed point value we're generating here is actually
      2^-33*(1 << 32) = 0.5, which truncates to 0 from both sides.
    */
    {-std::ldexp(1.0, -33), 0, 32},
    {std::ldexp(1.0, -33), 0, 32},

    // Min and max representable values.
    {-static_cast<double>(1ll << 31), S64_MIN, 32},
    {static_cast<double>((1ll << 31) - 1), ((1ll << 31) - 1) << 32, 32},
};
INSTANTIATE_TEST_SUITE_P(frac_bits_32, FixedConversionTestFixedFromDouble,
                         ValuesIn(from_double_frac_bits_32));

// frac_bits = 62
const FixedFromDoubleParam from_double_frac_bits_62[] = {
    // At infinite precision, these values would be +/-4150517416584649114, but
    // double rounds to 53 bits, so they become +/-4150517416584649216.
    {-0.9, -4150517416584649216LL, 62},
    {0.9, 4150517416584649216LL, 62},

    // Min and max representable values.
    {-2.0, S64_MIN, 62},
    {1.0, 1ll << 62, 62},
};
INSTANTIATE_TEST_SUITE_P(frac_bits_62, FixedConversionTestFixedFromDouble,
                         ValuesIn(from_double_frac_bits_62));

// Fixed -> Double
// ----------------------------------------------------------------------------

struct FixedToDoubleTestParam {
  s64 fixed_value;
  unsigned int frac_bits;
  double double_value;

  friend auto operator<<(std::ostream& out, const FixedToDoubleTestParam& src)
      -> std::ostream& {
    return out << "{" << src.fixed_value << ", " << src.frac_bits << ", "
               << src.double_value << "}";
  }
};

struct FixedConversionTestFixedToDouble
    : TestWithParam<FixedToDoubleTestParam> {};

TEST_P(FixedConversionTestFixedToDouble, to_double) {
  const auto param = GetParam();
  const auto actual =
      curves_fixed_to_double(param.fixed_value, param.frac_bits);
  ASSERT_DOUBLE_EQ(param.double_value, actual);
}

// frac_bits = 0
const FixedToDoubleTestParam to_double_frac_bits_0[] = {
    // frac_bits = 0 is just the original integers as doubles with no scaling.
    {123, 0, 123.0},
    {-456, 0, -456.0},
};
INSTANTIATE_TEST_SUITE_P(frac_bits_0, FixedConversionTestFixedToDouble,
                         ValuesIn(to_double_frac_bits_0));

// frac_bits = 32
const FixedToDoubleTestParam to_double_frac_bits_32[] = {
    // frac_bits = 32, normal values with full precision
    {(2ll << 32) | (1ll << 31), 32, 2.5},
    {(-3ll << 32) | (1ll << 31), 32, -2.5},
    {1, 32, std::ldexp(1.0, -32)},    // 1/2^32
    {-1, 32, -std::ldexp(1.0, -32)},  // -1/2^32
};
INSTANTIATE_TEST_SUITE_P(frac_bits_32, FixedConversionTestFixedToDouble,
                         ValuesIn(to_double_frac_bits_32));

/*
  frac_bits = 60

  60 bits of fixed-point precision suffers precision loss when converting to a
  53-bit double.

  In q3.60:
    (1ll << 60) is 1.0
    (1ll << 0) is 2^-60 (60 - 0 = 60)
    (1ll << 6) is 2^-54 (60 - 6 = 54)
    (1ll << 7) is 2^-53 (60 - 7 = 53)

  1 + 2^-60 will lose the 2^-60 part, (1ll << 0) bit is cleared
  1 + 2^-54 will lose the 2^-54 part, (1ll << 6) bit is cleared
  1 + 2^-53 will keep the 2^-53 part, (1ll << 7) bit is set
*/
const FixedToDoubleTestParam to_double_frac_bits_60[] = {
    {(1ll << 60) | (1ll << 0), 60, 1.0},  // The 2^-60 part is lost
    {(1ll << 60) | (1ll << 6), 60, 1.0},  // The 2^-54 part is lost
    {(1ll << 60) | (1ll << 7), 60, 1.0 + std::ldexp(1.0, -53)},  // bit is kept
};
INSTANTIATE_TEST_SUITE_P(frac_bits_60, FixedConversionTestFixedToDouble,
                         ValuesIn(to_double_frac_bits_60));

// ----------------------------------------------------------------------------
// Constants Test
// ----------------------------------------------------------------------------

struct ConstantsTestParam {
  s64 (*constant_func)(unsigned int);
  double expected_value;
  unsigned int frac_bits;
  double tolerance;

  friend auto operator<<(std::ostream& out, const ConstantsTestParam& src)
      -> std::ostream& {
    return out << "{" << src.expected_value << ", " << src.frac_bits << ", "
               << src.tolerance << "}";
  }
};

struct FixedConstantsTest : public TestWithParam<ConstantsTestParam> {};

TEST_P(FixedConstantsTest, verify_constants) {
  const auto param = GetParam();

  const auto actual_fixed = param.constant_func(param.frac_bits);
  const auto one_fixed = curves_fixed_const_1(param.frac_bits);

  const auto actual_double = double(actual_fixed) / double(one_fixed);

  if (param.tolerance == 0.0) {
    ASSERT_DOUBLE_EQ(param.expected_value, actual_double);
  } else {
    ASSERT_NEAR(param.expected_value, actual_double, param.tolerance);
  }
}

// 1
const ConstantsTestParam constants_1[] = {
    {curves_fixed_const_1, 1, CURVES_FIXED_1_FRAC_BITS, 0.0},
    {curves_fixed_const_1, 1, CURVES_FIXED_1_FRAC_BITS / 2, 0.0},
    {curves_fixed_const_1, 1, 1, 0.0},
};
INSTANTIATE_TEST_SUITE_P(constants_1, FixedConstantsTest,
                         ValuesIn(constants_1));

// e
const ConstantsTestParam constants_e[] = {
    {curves_fixed_const_e, M_E, CURVES_FIXED_E_FRAC_BITS, 0.0},
    {curves_fixed_const_e, M_E, CURVES_FIXED_E_FRAC_BITS / 2, 6.0e-10},
    {curves_fixed_const_e, M_E, 1, 2.2e-1},
};
INSTANTIATE_TEST_SUITE_P(constants_e, FixedConstantsTest,
                         ValuesIn(constants_e));

// ln(2)
const ConstantsTestParam constants_ln2[] = {
    {curves_fixed_const_ln2, std::log(2.0), CURVES_FIXED_LN2_FRAC_BITS, 0.0},
    {curves_fixed_const_ln2, std::log(2.0), CURVES_FIXED_LN2_FRAC_BITS / 2,
     4.3e-10},
    {curves_fixed_const_ln2, std::log(2.0), 1, 2.0e-1},
};
INSTANTIATE_TEST_SUITE_P(constants_ln2, FixedConstantsTest,
                         ValuesIn(constants_ln2));

// pi
const ConstantsTestParam constants_pi[] = {
    {curves_fixed_const_pi, M_PI, CURVES_FIXED_PI_FRAC_BITS, 0.0},
    {curves_fixed_const_pi, M_PI, CURVES_FIXED_PI_FRAC_BITS / 2, 1.3e-10},
    {curves_fixed_const_pi, M_PI, 1, 1.5e-1},
};
INSTANTIATE_TEST_SUITE_P(constants_pi, FixedConstantsTest,
                         ValuesIn(constants_pi));

// ----------------------------------------------------------------------------
// Multiplication Test
// ----------------------------------------------------------------------------

struct MultiplicationTestParams {
  s64 multiplicand;
  unsigned int multiplicand_frac_bits;
  s64 multiplier;
  unsigned int multiplier_frac_bits;
  unsigned int output_frac_bits;
  s64 expected_result;

  friend auto operator<<(std::ostream& out, const MultiplicationTestParams& src)
      -> std::ostream& {
    return out << "{" << src.multiplicand << ", " << src.multiplicand_frac_bits
               << ", " << src.multiplier << ", " << src.multiplier_frac_bits
               << ", " << src.output_frac_bits << ", " << src.expected_result
               << "}";
  }
};

struct FixedMultiplicationTest : TestWithParam<MultiplicationTestParams> {};

TEST_P(FixedMultiplicationTest, expected_result) {
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
TEST_P(FixedMultiplicationTest, multiplication_is_commutative) {
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
TEST_P(FixedMultiplicationTest, frac_bits_order_doesnt_matter) {
  const auto expected_result = GetParam().expected_result;

  const auto actual_result = curves_fixed_multiply(
      GetParam().multiplicand, GetParam().multiplier_frac_bits,
      GetParam().multiplier, GetParam().multiplicand_frac_bits,
      GetParam().output_frac_bits);

  ASSERT_EQ(expected_result, actual_result);
}

// Zero multiplied by anything yields zero, regardless of precision.
const MultiplicationTestParams multiplication_zero[] = {
    {0, 0, 0, 0, 0, 0},       // Zero precision
    {0, 32, 0, 32, 32, 0},    // Mid precision
    {0, 62, 5, 62, 62, 0},    // High precision, non-zero multiplier
    {100, 32, 0, 32, 32, 0},  // Non-zero multiplicand
};
INSTANTIATE_TEST_SUITE_P(zero, FixedMultiplicationTest,
                         ValuesIn(multiplication_zero));

// Multiplying by 1 should preserve the value (with rescaling).
const MultiplicationTestParams multiplication_identity[] = {
    // At zero precision: 2 * 1 = 2
    {2, 0, 1, 0, 0, 2},

    // At 32 bits: 5 * 1.0 = 5.0
    {5LL << 32, 32, 1LL << 32, 32, 32, 5LL << 32},

    // Different input precisions, same output precision
    {3LL << 16, 16, 1LL << 32, 32, 32, 3LL << 32},
};
INSTANTIATE_TEST_SUITE_P(identity, FixedMultiplicationTest,
                         ValuesIn(multiplication_identity));

// Simple integer multiplication (frac_bits = 0 for all).
const MultiplicationTestParams multiplication_integers[] = {
    {2, 0, 3, 0, 0, 6},   {5, 0, 7, 0, 0, 35},   {10, 0, 10, 0, 0, 100},
    {-2, 0, 3, 0, 0, -6}, {-5, 0, -7, 0, 0, 35},
};
INSTANTIATE_TEST_SUITE_P(integers, FixedMultiplicationTest,
                         ValuesIn(multiplication_integers));

// Basic fractional multiplication with simple, verifiable values.
const MultiplicationTestParams multiplication_simple_fractions[] = {
    // 2.0 * 3.0 = 6.0, all at q31.32
    {2LL << 32, 32, 3LL << 32, 32, 32, 6LL << 32},

    // 2.5 * 2.0 = 5.0, at q1.31 (2.5 = 5/2, so (5 << 31) / 2 = 2.5)
    {5LL << 30, 31, 2LL << 31, 31, 31, 5LL << 31},

    // 1.5 * 2.0 = 3.0, at q15.48
    {(3LL << 47), 48, (2LL << 48), 48, 48, 3LL << 48},

    // Negative: -2.0 * 3.0 = -6.0
    {-(2LL << 32), 32, 3LL << 32, 32, 32, -(6LL << 32)},
};
INSTANTIATE_TEST_SUITE_P(simple_fractions, FixedMultiplicationTest,
                         ValuesIn(multiplication_simple_fractions));

// Multiplying values with different input and output precisions.
const MultiplicationTestParams multiplication_precision_conversion[] = {
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
INSTANTIATE_TEST_SUITE_P(precision_conversion, FixedMultiplicationTest,
                         ValuesIn(multiplication_precision_conversion));

// Verify round-to-zero behavior when precision is reduced.
const MultiplicationTestParams multiplication_rounding[] = {
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
INSTANTIATE_TEST_SUITE_P(rounding, FixedMultiplicationTest,
                         ValuesIn(multiplication_rounding));

// Verify correct sign handling for all input sign combinations.
const MultiplicationTestParams multiplication_signs[] = {
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
INSTANTIATE_TEST_SUITE_P(signs, FixedMultiplicationTest,
                         ValuesIn(multiplication_signs));

// Verify saturation when the result is too large for s64.
const MultiplicationTestParams multiplication_saturation[] = {
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
INSTANTIATE_TEST_SUITE_P(saturation, FixedMultiplicationTest,
                         ValuesIn(multiplication_saturation));

// Large values that fit correctly without saturating.
const MultiplicationTestParams multiplication_boundaries[] = {
    // Values that are large but whose product fits in s64
    // For q31.32: max safe value is roughly sqrt(S64_MAX >> 32)
    // That's about sqrt(2^31) = 2^15.5 ≈ 46340
    {46340LL << 32, 32, 46340LL << 32, 32, 32, (46340LL * 46340LL) << 32},

    // At integer precision: smaller values
    {1000000, 0, 1000000, 0, 0, 1000000000000LL},

    // Negative boundaries
    {-46340LL << 32, 32, 46340LL << 32, 32, 32, -(46340LL * 46340LL) << 32},

    // One value at maximum, other small
    {S64_MAX, 0, 1, 0, 0, S64_MAX},
    {S64_MIN, 0, 1, 0, 0, S64_MIN},
};
INSTANTIATE_TEST_SUITE_P(boundaries, FixedMultiplicationTest,
                         ValuesIn(multiplication_boundaries));

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
  s64 dividend;
  s64 divisor;
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
const DivideOptimalShiftParams shift_basics[] = {
    // 1 / 1 -> Shift 62.
    // Check: (1 << 62) / 1 = 2^62 (Fits in s64 positive range)
    {1, 1, 62},

    // 1 / 2 -> Shift 63.
    // Divisor is larger (clz=62), so we can shift dividend more.
    // 62 + 63 - 62 = 63.
    {1, 2, 63},

    // 2 / 1 -> Shift 61.
    // Dividend is larger (clz=62), need to shift less to avoid overflow.
    // 62 + 62 - 63 = 61.
    {2, 1, 61},

    // 100 / 10 -> Shift 62.
    // 62 + clz(100) - clz(10) -> 62 + 57 - 60 = 59?
    // Let's recheck math:
    // clz(100) = 57. clz(10) = 60.
    // 62 + 57 - 60 = 59.
    {100, 10, 59},
};
INSTANTIATE_TEST_SUITE_P(Basics, DivideOptimalShiftTest,
                         ValuesIn(shift_basics));

/*
  Zero Dividend (The | 1 Trick)

  Verifies that the branchless fix works and treats 0 exactly like 1.
*/
const DivideOptimalShiftParams shift_zeros[] = {
    // 0 / 1.
    // Internal logic: clz(0 | 1) -> clz(1) -> 63.
    // Result: 62 + 63 - 63 = 62.
    {0, 1, 62},

    // 0 / S64_MAX.
    // clz(dividend) = 63. clz(divisor) = 1.
    // 62 + 63 - 1 = 124.
    {0, S64_MAX, 124},
};
INSTANTIATE_TEST_SUITE_P(Zeros, DivideOptimalShiftTest, ValuesIn(shift_zeros));

/*
  Sign Invariance

  Verifies abs() is working. The shift should only depend on magnitude.
*/
const DivideOptimalShiftParams shift_signs[] = {
    // 1 / -1 -> Same as 1 / 1 -> 62
    {1, -1, 62},

    // -1 / 1 -> Same as 1 / 1 -> 62
    {-1, 1, 62},

    // -1 / -1 -> Same as 1 / 1 -> 62
    {-1, -1, 62},
};
INSTANTIATE_TEST_SUITE_P(Signs, DivideOptimalShiftTest, ValuesIn(shift_signs));

/*
  Extremes and Overflows

  Testing the boundaries of s64.
*/
const DivideOptimalShiftParams shift_extremes[] = {
    // S64_MAX / 1
    // clz(MAX) = 1. clz(1) = 63.
    // 62 + 1 - 63 = 0.
    // (We can't shift S64_MAX left at all, valid)
    {S64_MAX, 1, 0},

    // S64_MIN / 1 (The Edge Case)
    // clz(MIN) = 0. clz(1) = 63.
    // 62 + 0 - 63 = -1.
    // (Correctly identifies that S64_MIN / 1 requires saturation/checks)
    {S64_MIN, 1, -1},

    // 1 / S64_MAX
    // clz(1) = 63. clz(MAX) = 1.
    // 62 + 63 - 1 = 124.
    // (We can shift 1 left by 124 bits safely inside s128)
    {1, S64_MAX, 124},

    // S64_MAX / S64_MAX
    // 62 + 1 - 1 = 62.
    {S64_MAX, S64_MAX, 62},
};
INSTANTIATE_TEST_SUITE_P(Extremes, DivideOptimalShiftTest,
                         ValuesIn(shift_extremes));

// ----------------------------------------------------------------------------
// curves_fixed_divide()
// ----------------------------------------------------------------------------

struct DivisionTestParams {
  s64 dividend;
  unsigned int dividend_frac_bits;
  s64 divisor;
  unsigned int divisor_frac_bits;
  unsigned int output_frac_bits;
  s64 expected_result;

  friend auto operator<<(std::ostream& out, const DivisionTestParams& src)
      -> std::ostream& {
    return out << "{" << src.dividend << ", " << src.dividend_frac_bits << ", "
               << src.divisor << ", " << src.divisor_frac_bits << ", "
               << src.output_frac_bits << ", " << src.expected_result << "}";
  }
};

struct FixedDivisionTest : TestWithParam<DivisionTestParams> {};

TEST_P(FixedDivisionTest, expected_result) {
  const auto expected_result = GetParam().expected_result;

  const auto actual_result = curves_fixed_divide(
      GetParam().dividend, GetParam().dividend_frac_bits, GetParam().divisor,
      GetParam().divisor_frac_bits, GetParam().output_frac_bits);

  ASSERT_EQ(expected_result, actual_result);
}

/*
  Basic Integer Math

  Sanity checks ensuring the basic plumbing works with 0 scaling.
*/
const DivisionTestParams divide_basics[] = {
    // 100 / 2 = 50
    {100, 0, 2, 0, 0, 50},
    // 100 / -2 = -50 (Sign logic)
    {100, 0, -2, 0, 0, -50},
    // -100 / 2 = -50
    {-100, 0, 2, 0, 0, -50},
    // -100 / -2 = 50
    {-100, 0, -2, 0, 0, 50},
    // 0 / 50 = 0 (The Dividend=0 edge case, verifying the |1 trick works)
    {0, 0, 50, 0, 0, 0},
    // Identity: 123 / 1 = 123
    {123, 0, 1, 0, 0, 123},
};
INSTANTIATE_TEST_SUITE_P(basics, FixedDivisionTest, ValuesIn(divide_basics));

/*
  Precision & Scaling

  Tests that output_frac_bits correctly shifts the result relative to input
  scales.
*/
const DivisionTestParams divide_scaling[] = {
    // 1 / 2 = 0.5 (Result requested in Q1) -> stored as 1
    {1, 0, 2, 0, 1, 1},

    // 1 / 2 = 0.5 (Result requested in Q32) -> stored as 0.5 * 2^32
    {1, 0, 2, 0, 32, 2147483648LL},

    // Mixed Input Scales:
    // Dividend 1.0 (Q16 representation: 65536)
    // Divisor  0.5 (Q17 representation: 65536)
    // Math: 1.0 / 0.5 = 2.0.
    // Result requested in Q0 -> 2
    {65536, 16, 65536, 17, 0, 2},

    // Same inputs, Result requested in Q4 -> 2.0 * 16 = 32
    {65536, 16, 65536, 17, 4, 32},
};
INSTANTIATE_TEST_SUITE_P(scaling, FixedDivisionTest, ValuesIn(divide_scaling));

/*
  Optimal Shift (Precision Preservation)

  These tests fail if you naively shift to total_shift. They require the
  intermediate calculation to be "super scaled" to preserve bits.
*/
const DivisionTestParams divide_high_precision[] = {
    // 1 / 3 approx 0.3333...
    // If we shifted just enough for Q2 output, we'd calculate 100/11 = 0.
    // We need internal precision to capture the fraction.
    // 1 / 3 in Q16 -> 21845 (0.333328...)
    {1, 0, 3, 0, 16, 21845},

    // Extreme case: Small / Large -> High Precision Output
    // 1 / 1,000,000. Output requested in Q40.
    // Target: 1e-6 * 2^40 = 1,099,511.6... -> 1,099,511 (RTZ)
    {1, 0, 1000000, 0, 40, 1099511},
};
INSTANTIATE_TEST_SUITE_P(HighPrecision, FixedDivisionTest,
                         ValuesIn(divide_high_precision));

/*
  Rounding (RTZ)

  Verifies that we are strictly rounding toward zero, not nearest, and not
  floor (for negatives).
*/
const DivisionTestParams divide_rounding[] = {
    // Positive Truncation: 2 / 3 = 0.66... -> 0 in Q0
    {2, 0, 3, 0, 0, 0},

    // Negative Truncation (RTZ check):
    // -2 / 3 = -0.66...
    // Floor would be -1. RTZ should be 0.
    {-2, 0, 3, 0, 0, 0},

    // Precision Downshift Rounding:
    // 10 / 3 = 3.333...
    // Calculated high precision, then shifted down to Q0.
    // Should be 3.
    {10, 0, 3, 0, 0, 3},

    // Negative Downshift:
    // -10 / 3 = -3.333...
    // Should be -3.
    {-10, 0, 3, 0, 0, -3},
};
INSTANTIATE_TEST_SUITE_P(Rounding, FixedDivisionTest,
                         ValuesIn(divide_rounding));

/*
  Saturation

  Verifies the overflow guards.
*/
const DivisionTestParams divide_saturation[] = {
    // S64_MAX / 0.5 (Divisor is 1 in Q1) -> S64_MAX * 2 -> Overflow
    // Should return S64_MAX
    {S64_MAX, 0, 1, 1, 0, S64_MAX},

    // S64_MIN / 0.5 -> S64_MIN * 2 -> Underflow
    // Should return S64_MIN
    {S64_MIN, 0, 1, 1, 0, S64_MIN},

    // S64_MIN / -1 -> S64_MAX + 1 -> Overflow
    // Should return S64_MAX
    {S64_MIN, 0, -1, 0, 0, S64_MAX},

    // Just barely overflowing:
    // (2^62) / 0.5 -> 2^63 -> Overflow
    {1LL << 62, 0, 1, 1, 0, S64_MAX},
};
INSTANTIATE_TEST_SUITE_P(Saturation, FixedDivisionTest,
                         ValuesIn(divide_saturation));

}  // namespace
}  // namespace curves
