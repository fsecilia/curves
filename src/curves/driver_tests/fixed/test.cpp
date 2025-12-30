// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <curves/testing/test.hpp>
#include <curves/math/fixed.hpp>
#include <curves/math/io.hpp>

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
    {-4611686018427387903LL, 61, -2},  // < -2, floors to -2, truncates to -1
    {-3458764513820540928LL, 61, -2},  // = -1.5, floors to -2, truncates to -1
    {-3458764513820540927LL, 61, -1},  // < -1.5, floors to -2, truncates to -1
    {-2305843009213693952LL, 61, -1},  // = -1, floors to -1, truncates to -1
    {-2305843009213693951LL, 61, -1},  // < -1, floors to -1, truncates to 0
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
    {2305843009213693951LL, 61, 1},  // < 1, floors to 0, truncates to 0
    {2305843009213693952LL, 61, 1},  // = 1, floors to 1, truncates to 1
    {3458764513820540927LL, 61, 1},  // < 1.5, floors to 1, truncates to 1
    {3458764513820540928LL, 61, 2},  // = 1.5, floors to 1, truncates to 1
    {4611686018427387903LL, 61, 2},  // < 2, floors to 1, truncates to 1
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
    {S64_MIN, 1, S64_MIN >> 1},           {S64_MIN + 1, 1, S64_MIN >> 1},
    {S64_MAX - 2, 1, (S64_MAX >> 1) - 1}, {S64_MAX - 1, 1, S64_MAX >> 1},
    {S64_MAX, 1, (S64_MAX >> 1) + 1},
};
INSTANTIATE_TEST_SUITE_P(edge_case_1, FixedTestRoundingIntegerConversion,
                         ValuesIn(round_ints_edge_case_1));

// frac_bits = 32: Typical precision
const RoundingIntegerConversionParam round_ints_edge_case_32[] = {
    {S64_MIN, 32, S64_MIN >> 32},
    {S64_MIN + 1, 32, (S64_MIN >> 32)},
    {S64_MAX - (1LL << 32), 32, (S64_MAX >> 32)},
    {S64_MAX - (1LL << 32) + 1, 32, (S64_MAX >> 32)},
    {S64_MAX, 32, (S64_MAX >> 32) + 1},
};
INSTANTIATE_TEST_SUITE_P(edge_case_32, FixedTestRoundingIntegerConversion,
                         ValuesIn(round_ints_edge_case_32));

// frac_bits = 61: Highest precision that doesn't hit range boundary.
const RoundingIntegerConversionParam round_ints_edge_case_61[] = {
    {S64_MIN, 61, -4},
    {S64_MIN + 1, 61, -4},
    {S64_MAX - (1LL << 61), 61, 3},
    {S64_MAX - (1LL << 61) + 1, 61, 3},
    {S64_MAX, 61, 4},
};
INSTANTIATE_TEST_SUITE_P(edge_case_61, FixedTestRoundingIntegerConversion,
                         ValuesIn(round_ints_edge_case_61));

// frac_bits = 62: Maximum precision
const RoundingIntegerConversionParam round_ints_edge_case_62[] = {
    {S64_MIN, 62, -2},
    {S64_MIN + 1, 62, -2},
    {S64_MAX - (1LL << 62), 62, 1},
    {S64_MAX - (1LL << 62) + 1, 62, 1},
    {S64_MAX, 62, 2},
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

// 1.5
const ConstantsTestParam constants_1_5[] = {
    {curves_fixed_const_1_5, 1.5, CURVES_FIXED_1_5_FRAC_BITS, 0.0},
    {curves_fixed_const_1_5, 1.5, CURVES_FIXED_1_5_FRAC_BITS / 2, 0.0},
    {curves_fixed_const_1_5, 1.5, 1, 0.0},
};
INSTANTIATE_TEST_SUITE_P(constants_1_5, FixedConstantsTest,
                         ValuesIn(constants_1_5));

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
// curves_fixed_fma()
// ----------------------------------------------------------------------------

struct FmaParams {
  s64 multiplicand;
  unsigned int multiplicand_frac_bits;
  s64 multiplier;
  unsigned int multiplier_frac_bits;
  s64 addend;
  unsigned int addend_frac_bits;
  unsigned int output_frac_bits;
  s64 expected_result;

  friend auto operator<<(std::ostream& out, const FmaParams& src)
      -> std::ostream& {
    return out << "{" << src.multiplicand << ", " << src.multiplicand_frac_bits
               << ", " << src.multiplier << ", " << src.multiplier_frac_bits
               << ", " << src.addend << ", " << src.addend_frac_bits << ", "
               << src.expected_result << " } ";
  }
};

struct FmaTest : TestWithParam<FmaParams> {};

TEST_P(FmaTest, expected_result) {
  const auto actual_result = curves_fixed_fma(
      GetParam().multiplicand, GetParam().multiplicand_frac_bits,
      GetParam().multiplier, GetParam().multiplier_frac_bits, GetParam().addend,
      GetParam().addend_frac_bits, GetParam().output_frac_bits);

  ASSERT_EQ(GetParam().expected_result, actual_result);
}

/*
  Smoke Tests

  Baseline sanity checks.
*/
const FmaParams fma_smoke_tests[] = {
    // 2.0*3.0 + 1.0 = 7.0
    {2LL << 32, 32, 3LL << 32, 32, 1LL << 32, 32, 32, 7LL << 32},

    // 1.5*1.5 + 2.0 = 4.25 in Q16.16
    {1610612736LL, 30, 1610612736LL, 30, 2LL << 32, 32, 16, 278528LL},

    // 2.0*4.0 + 1/2^32 = 8.0 + 1/2^32, at Q59.4, this round back to 8.
    {2LL << 4, 4, 4LL << 4, 4, 1LL, 32, 4, 128LL},

    // 2.0*4.0 + 1/2^32 = 8.0 + 1/2^32.
    {2LL << 4, 4, 4LL << 4, 4, 1LL, 32, 32, (8LL << 32) + 1},

    // 2.5*1.0 + 0.0 = 2.5, tie breaker rounds down to 2
    {5LL, 1, 1LL, 0, 0LL, 0, 0, 2LL},

    // 3.5*1.0 + 0.0 = 3.5, tie breaker rounds up to 4
    {7LL, 1, 1LL, 0, 0LL, 0, 0, 4LL},
};
INSTANTIATE_TEST_SUITE_P(smoke_tests, FmaTest, ValuesIn(fma_smoke_tests));

}  // namespace
}  // namespace curves
