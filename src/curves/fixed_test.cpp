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
// curves_fixed_multiply()
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
  Invalid Fractional Bits

  Tests that frac_bits >= 64 triggers the error handler correctly.
*/
const DivisionTestParams divide_invalid_frac_bits[] = {
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
INSTANTIATE_TEST_SUITE_P(invalid_frac_bits, FixedDivisionTest,
                         ValuesIn(divide_invalid_frac_bits));

/*
  Zero Dividend

  Zero divided by anything (except 0) should always yield 0, regardless of
  precision settings. The optimal shift logic handles 0 with the (| 1) trick.
*/
const DivisionTestParams divide_zero_dividend[] = {
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
INSTANTIATE_TEST_SUITE_P(zero_dividend, FixedDivisionTest,
                         ValuesIn(divide_zero_dividend));

/*
  Division by Zero

  Should saturate based on dividend sign. Positive dividends -> S64_MAX,
  negative dividends -> S64_MIN.
*/
const DivisionTestParams divide_by_zero[] = {
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
INSTANTIATE_TEST_SUITE_P(division_by_zero, FixedDivisionTest,
                         ValuesIn(divide_by_zero));

/*
  Division by One

  Dividing by 1 should preserve the value after rescaling to output precision.
*/
const DivisionTestParams divide_by_one[] = {
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
INSTANTIATE_TEST_SUITE_P(identity, FixedDivisionTest, ValuesIn(divide_by_one));

/*
  Simple Integer Cases

  Basic division with frac_bits = 0 for all parameters.
*/
const DivisionTestParams divide_integers[] = {
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
INSTANTIATE_TEST_SUITE_P(integers, FixedDivisionTest,
                         ValuesIn(divide_integers));

/*
  All Sign Combinations

  Verifies correct sign handling for quotients: positive/positive = positive,
  positive/negative = negative, negative/positive = negative,
  negative/negative = positive.
*/
const DivisionTestParams divide_signs[] = {
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
INSTANTIATE_TEST_SUITE_P(signs, FixedDivisionTest, ValuesIn(divide_signs));

/*
  Output Precision Greater Than Input Precision

  Tests where output_frac_bits > dividend_frac_bits + optimal_shift -
  divisor_frac_bits, requiring a left shift of the quotient.
*/
const DivisionTestParams divide_precision_upscale[] = {
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
INSTANTIATE_TEST_SUITE_P(precision_upscale, FixedDivisionTest,
                         ValuesIn(divide_precision_upscale));

/*
  Output Precision Less Than Input Precision

  Tests where:
    output_frac_bits < dividend_frac_bits + optimal_shift - divisor_frac_bits,
  requiring a right shift of the quotient with round-to-zero behavior.
*/
const DivisionTestParams divide_precision_downscale[] = {
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
INSTANTIATE_TEST_SUITE_P(precision_downscale, FixedDivisionTest,
                         ValuesIn(divide_precision_downscale));

/*
  All Precisions Equal

  When dividend_frac_bits = divisor_frac_bits = output_frac_bits, the
  scaling should be relatively straightforward.
*/
const DivisionTestParams divide_equal_precision[] = {
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
INSTANTIATE_TEST_SUITE_P(equal_precision, FixedDivisionTest,
                         ValuesIn(divide_equal_precision));

/*
  Optimal Shift Equals Zero

  When the dividend is large enough that no left shift can be applied without
  risking overflow. This happens when clz(dividend) <= clz(divisor) + 1.
*/
const DivisionTestParams divide_optimal_shift_zero[] = {
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
INSTANTIATE_TEST_SUITE_P(optimal_shift_zero, FixedDivisionTest,
                         ValuesIn(divide_optimal_shift_zero));

/*
  Optimal Shift Equals Negative One

  The special S64_MIN / 1 case where clz(S64_MIN) = 0, resulting in
  optimal_shift = 62 + 0 - 63 = -1, which should saturate.
*/
const DivisionTestParams divide_optimal_shift_negative[] = {
    // S64_MIN / 1 should saturate to S64_MIN (same sign)
    {S64_MIN, 0, 1, 0, 0, S64_MIN},

    // S64_MIN / -1 should saturate to S64_MAX (different signs)
    {S64_MIN, 0, -1, 0, 0, S64_MAX},

    // With fractional bits
    {S64_MIN, 0, 1, 0, 32, S64_MIN},
    {S64_MIN, 0, -1, 0, 32, S64_MAX},
    {S64_MIN, 32, 1LL << 32, 32, 32, S64_MIN},
};
INSTANTIATE_TEST_SUITE_P(optimal_shift_negative, FixedDivisionTest,
                         ValuesIn(divide_optimal_shift_negative));

/*
  High Optimal Shift Values

  Small dividend and/or large divisor produce high optimal shift values,
  maximizing intermediate precision before division.
*/
const DivisionTestParams divide_optimal_shift_maximum[] = {
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
INSTANTIATE_TEST_SUITE_P(optimal_shift_maximum, FixedDivisionTest,
                         ValuesIn(divide_optimal_shift_maximum));

/*
  Remaining Shift Exceeds 63

  When remaining_shift > 63, the result would require shifting beyond s64
  capacity, so the function saturates based on quotient sign.
*/
const DivisionTestParams divide_remaining_shift_overflow[] = {
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
INSTANTIATE_TEST_SUITE_P(remaining_shift_overflow, FixedDivisionTest,
                         ValuesIn(divide_remaining_shift_overflow));

/*
  Remaining Shift Below -63

  When remaining_shift < -63, the result would be shifted so far right that
  it becomes 0.
*/
const DivisionTestParams divide_remaining_shift_underflow[] = {
    // Large intermediate precision, no output precision
    {1, 62, S64_MAX, 0, 0, 0},
    {100, 60, 1LL << 62, 0, 0, 0},
    {1LL << 62, 62, 1, 0, 0, 1},

    // Various combinations that produce massive right shifts
    {1, 60, 1LL << 60, 0, 0, 0},
    {1, 62, 1LL << 30, 0, 0, 0},
};
INSTANTIATE_TEST_SUITE_P(remaining_shift_underflow, FixedDivisionTest,
                         ValuesIn(divide_remaining_shift_underflow));

/*
  Rounding Positive Fractions

  When a positive result has a fractional part, it should truncate toward zero.
*/
const DivisionTestParams divide_rtz_positive[] = {
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
INSTANTIATE_TEST_SUITE_P(rtz_positive, FixedDivisionTest,
                         ValuesIn(divide_rtz_positive));

/*
  Rounding Negative Fractions

  When a negative result has a fractional part, it should truncate toward zero
  (not toward negative infinity like floor division).
*/
const DivisionTestParams divide_rtz_negative[] = {
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
INSTANTIATE_TEST_SUITE_P(rtz_negative, FixedDivisionTest,
                         ValuesIn(divide_rtz_negative));

/*
  Results That Round to Zero

  Division operations where the mathematical result is very small and rounds
  to zero at the requested output precision.
*/
const DivisionTestParams divide_near_zero[] = {
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
INSTANTIATE_TEST_SUITE_P(near_zero, FixedDivisionTest,
                         ValuesIn(divide_near_zero));

/*
  Positive Results That Overflow

  Division operations that would produce results exceeding S64_MAX.
*/
const DivisionTestParams divide_saturate_positive[] = {
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
INSTANTIATE_TEST_SUITE_P(saturate_positive, FixedDivisionTest,
                         ValuesIn(divide_saturate_positive));

/*
  Negative Results That Overflow

  Division operations that would produce results below S64_MIN.
*/
const DivisionTestParams divide_saturate_negative[] = {
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
INSTANTIATE_TEST_SUITE_P(saturate_negative, FixedDivisionTest,
                         ValuesIn(divide_saturate_negative));

/*
  S64_MIN Edge Cases

  S64_MIN is special because it's the only value where abs(x) doesn't fit
  in the positive range of s64. Tests various operations involving S64_MIN.
*/
const DivisionTestParams divide_s64_min_special[] = {
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
INSTANTIATE_TEST_SUITE_P(s64_min_special, FixedDivisionTest,
                         ValuesIn(divide_s64_min_special));

/*
  S64_MAX Edge Cases

  Tests involving the maximum positive s64 value.
*/
const DivisionTestParams divide_s64_max_cases[] = {
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
INSTANTIATE_TEST_SUITE_P(s64_max_cases, FixedDivisionTest,
                         ValuesIn(divide_s64_max_cases));

/*
  Maximum Safe Precision

  Tests using the highest safe fractional bit counts for the input values.
*/
const DivisionTestParams divide_high_precision[] = {
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
INSTANTIATE_TEST_SUITE_P(high_precision, FixedDivisionTest,
                         ValuesIn(divide_high_precision));

/*
  Practical Real-World Cases

  Division operations that might appear in actual fixed-point calculations,
  with realistic precision combinations.
*/
const DivisionTestParams divide_realistic[] = {
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
INSTANTIATE_TEST_SUITE_P(realistic, FixedDivisionTest,
                         ValuesIn(divide_realistic));

}  // namespace
}  // namespace curves
