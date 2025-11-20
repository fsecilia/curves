// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <curves/test.hpp>
#include <curves/fixed.hpp>
#include <curves/io.hpp>

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

}  // namespace
}  // namespace curves
