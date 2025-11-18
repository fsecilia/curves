// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia

  This file contains tests for s128 versions of functions that also have an s64
  version.
*/

#include "fixed.hpp"
#include "fixed_io.hpp"
#include <curves/test.hpp>

#define S128_MAX CURVES_S128_MAX
#define S128_MIN CURVES_S128_MIN

namespace curves {
namespace {

// ----------------------------------------------------------------------------
// curves_saturate_s128
// ----------------------------------------------------------------------------

TEST(curves_saturate_s128, negative) {
  ASSERT_EQ(S128_MIN, curves_saturate_s128(false));
}

TEST(curves_saturate_s128, positive) {
  ASSERT_EQ(S128_MAX, curves_saturate_s128(true));
}

// ----------------------------------------------------------------------------
// __curves_fixed_rescale_error_s128
// ----------------------------------------------------------------------------

struct FixedRescaleErrorS128TestParam {
  s128 value;
  unsigned int frac_bits;
  unsigned int output_frac_bits;
  s128 expected_result;

  friend auto operator<<(std::ostream& out,
                         const FixedRescaleErrorS128TestParam& src)
      -> std::ostream& {
    return out << "{" << src.value << ", " << src.frac_bits << ", "
               << src.output_frac_bits << ", " << src.expected_result << "}";
  }
};

struct FixedRescaleErrorS128Test
    : TestWithParam<FixedRescaleErrorS128TestParam> {};

TEST_P(FixedRescaleErrorS128Test, expected_result) {
  const auto actual_result = __curves_fixed_rescale_error_s128(
      GetParam().value, GetParam().frac_bits, GetParam().output_frac_bits);

  ASSERT_EQ(GetParam().expected_result, actual_result);
}

/*
  Tests zero-value inputs. These always return zero regardless of shift
  direction or precision, since zero can't overflow.
*/
const FixedRescaleErrorS128TestParam rescale_error_s128_zero_values[] = {
    {0, 0, 0, 0},  // All fractional bits zero
    {0, 1, 1, 0},  // No shift, nonzero fractional bits
    {0, 1, 0, 0},  // Right shift
    {0, 0, 1, 0},  // Left shift
};
INSTANTIATE_TEST_SUITE_P(zero_values, FixedRescaleErrorS128Test,
                         ValuesIn(rescale_error_s128_zero_values));

/*
  Tests right shift cases, output_frac_bits < frac_bits. The error handler
  returns zero for right shifts regardless of the input value, since right
  shifts reduce magnitude and cannot cause overflow.
*/
const FixedRescaleErrorS128TestParam rescale_error_s128_shr[] = {
    {-1, 1, 0, 0},
    {1, 1, 0, 0},
};
INSTANTIATE_TEST_SUITE_P(right_shifts, FixedRescaleErrorS128Test,
                         ValuesIn(rescale_error_s128_shr));

/*
  Tests no-shift cases, output_frac_bits == frac_bits, with non-zero values.
  When an invalid number of fractional bits cause the error handler to be
  called with no shift required, non-zero values saturate based on their sign.
*/
const FixedRescaleErrorS128TestParam rescale_error_s128_no_shift_sat[] = {
    {1, 0, 0, S128_MAX},   // Positive saturates to max
    {-1, 0, 0, S128_MIN},  // Negative saturates to min
};
INSTANTIATE_TEST_SUITE_P(no_shift_saturation, FixedRescaleErrorS128Test,
                         ValuesIn(rescale_error_s128_no_shift_sat));

/*
  Tests left shift cases, output_frac_bits > frac_bits, with non-zero values.
  Left shifts that trigger the error handler cause saturation based on sign.
  Tests include both regular values and boundary values at S128_MAX/S128_MIN.
*/
const FixedRescaleErrorS128TestParam rescale_error_s128_shl_sat[] = {
    {1, 0, 1, S128_MAX},         // Positive regular value
    {-1, 0, 1, S128_MIN},        // Negative regular value
    {S128_MAX, 0, 1, S128_MAX},  // Positive boundary value
    {S128_MIN, 0, 1, S128_MIN},  // Negative boundary value
};
INSTANTIATE_TEST_SUITE_P(left_shift_saturation, FixedRescaleErrorS128Test,
                         ValuesIn(rescale_error_s128_shl_sat));

// ----------------------------------------------------------------------------
// __curves_fixed_shr_rtz_s128
// ----------------------------------------------------------------------------

// Tests values -1, 0, 1 at various shifts.
struct FixedShrRtzS128NearZeroTest : TestWithParam<unsigned int> {
  unsigned int shift = GetParam();
};

// The first value before 0 should round up to zero.
TEST_P(FixedShrRtzS128NearZeroTest, predecessor_rounds_up_towards_zero) {
  s128 value = -1;  // -1/divisor
  s128 expected = 0;
  ASSERT_EQ(expected, __curves_fixed_shr_rtz_s128(value, shift));
}

// 0 is a multiple of divisor, so it should not round in either direction.
TEST_P(FixedShrRtzS128NearZeroTest, exact_stays_zero) {
  s128 value = 0;  // 0 exactly
  s128 expected = 0;
  ASSERT_EQ(expected, __curves_fixed_shr_rtz_s128(value, shift));
}

// The first value after 0 should round down.
TEST_P(FixedShrRtzS128NearZeroTest, successor_rounds_down_towards_zero) {
  s128 value = 1;  // 1/divisor
  s128 expected = 0;
  ASSERT_EQ(expected, __curves_fixed_shr_rtz_s128(value, shift));
}
INSTANTIATE_TEST_SUITE_P(various_shifts, FixedShrRtzS128NearZeroTest,
                         Values(1, 16, 32, 64, 126, 127));

// ----------------------------------------------------------------------------

// Tests shifts and scales that aren't boundary conditions.
struct FixedShrRtzS128CommonCasesTestParam {
  unsigned int shift;
  s128 scale;

  friend auto operator<<(std::ostream& out,
                         const FixedShrRtzS128CommonCasesTestParam& src)
      -> std::ostream& {
    return out << "{" << src.shift << ", " << src.scale << "}";
  }
};

struct FixedShrRtzS128CommonCasesTest
    : TestWithParam<FixedShrRtzS128CommonCasesTestParam> {
  unsigned int shift = GetParam().shift;
  s128 scale = GetParam().scale;
  s128 divisor = static_cast<s128>(1) << shift;
};

// The first value before a negative multiple of divisor should round up.
TEST_P(FixedShrRtzS128CommonCasesTest,
       negative_predecessor_rounds_up_towards_zero) {
  const auto value = -scale * divisor - 1;  // -scale - 1/divisor
  const auto expected = -scale;

  const auto actual = __curves_fixed_shr_rtz_s128(value, shift);

  ASSERT_EQ(expected, actual);
}

// Exact multiples shouldn't round; there's no fractional part to handle.
TEST_P(FixedShrRtzS128CommonCasesTest, negative_exact_multiple_no_rounding) {
  s128 value = -scale * divisor;  // -scale exactly
  s128 expected = -scale;
  ASSERT_EQ(expected, __curves_fixed_shr_rtz_s128(value, shift));
}

// The first value after a negative multiple of divisor should round up.
TEST_P(FixedShrRtzS128CommonCasesTest,
       negative_successor_rounds_up_towards_zero) {
  s128 value = -scale * divisor + 1;  // -scale + 1/divisor
  s128 expected = -scale + 1;
  ASSERT_EQ(expected, __curves_fixed_shr_rtz_s128(value, shift));
}

// The first value before a positive multiple of divisor should round down.
TEST_P(FixedShrRtzS128CommonCasesTest,
       positive_predecessor_rounds_down_towards_zero) {
  const auto value = scale * divisor - 1;  // scale - 1/divisor
  const auto expected = scale - 1;

  const auto actual = __curves_fixed_shr_rtz_s128(value, shift);

  ASSERT_EQ(expected, actual);
}

// Exact multiples shouldn't round; there's no fractional part to handle.
TEST_P(FixedShrRtzS128CommonCasesTest, positive_exact_multiple_no_rounding) {
  s128 value = scale * divisor;  // scale exactly
  s128 expected = scale;
  ASSERT_EQ(expected, __curves_fixed_shr_rtz_s128(value, shift));
}

// The first value after a positive multiple of divisor should round down.
TEST_P(FixedShrRtzS128CommonCasesTest,
       positive_successor_rounds_down_towards_zero) {
  s128 value = scale * divisor + 1;  // scale + 1/divisor
  s128 expected = scale;
  ASSERT_EQ(expected, __curves_fixed_shr_rtz_s128(value, shift));
}

const FixedShrRtzS128CommonCasesTestParam shr_rtz_s128_shift_1[] = {
    {1, 1},                                  // unity
    {1, 2},                                  // smallest nonunity multiplier
    {1, 3},                                  // small odd multiplier
    {1, 1LL << 32},                          // large multiplier
    {1, (static_cast<s128>(1) << 125) - 1},  // very large odd multiplier
    {1, static_cast<s128>(1) << 125},        // max scale for this shift
};
INSTANTIATE_TEST_SUITE_P(shift_1, FixedShrRtzS128CommonCasesTest,
                         ValuesIn(shr_rtz_s128_shift_1));

const FixedShrRtzS128CommonCasesTestParam shr_rtz_s128_shift_16[] = {
    {16, 1},                // unity
    {16, 2},                // smallest nonunity multiplier
    {16, 3},                // small odd multiplier
    {16, 1LL << 24},        // large multiplier
    {16, (1LL << 47) - 1},  // max scale for this shift
};
INSTANTIATE_TEST_SUITE_P(shift_16, FixedShrRtzS128CommonCasesTest,
                         ValuesIn(shr_rtz_s128_shift_16));

const FixedShrRtzS128CommonCasesTestParam shr_rtz_s128_shift_32[] = {
    {32, 1},                // unity
    {32, 2},                // smallest nonunity multiplier
    {32, 3},                // small odd multiplier
    {32, (1LL << 16)},      // representative multiplier
    {32, (1LL << 31) - 1},  // max scale for this shift

    {126, 1},  // 126 has no room for scales
};
INSTANTIATE_TEST_SUITE_P(shift_32, FixedShrRtzS128CommonCasesTest,
                         ValuesIn(shr_rtz_s128_shift_32));

const FixedShrRtzS128CommonCasesTestParam shr_rtz_s128_shift_126[] = {
    {126, 1},  // 126 has no room for scales
};
INSTANTIATE_TEST_SUITE_P(shift_126, FixedShrRtzS128CommonCasesTest,
                         ValuesIn(shr_rtz_s128_shift_126));

// ----------------------------------------------------------------------------

// Tests specific edge cases.
struct FixedShrRtzS128EdgeCasesTestParam {
  s128 value;
  unsigned int shift;
  s128 expected_result;

  friend auto operator<<(std::ostream& out,
                         const FixedShrRtzS128EdgeCasesTestParam& src)
      -> std::ostream& {
    return out << "{" << src.value << ", " << src.shift << ", "
               << src.expected_result << "}";
  }
};

struct FixedShrRtzS128EdgeCasesTest
    : TestWithParam<FixedShrRtzS128EdgeCasesTestParam> {
  s128 value = GetParam().value;
  unsigned int shift = GetParam().shift;
  s128 expected_result = GetParam().expected_result;
};

TEST_P(FixedShrRtzS128EdgeCasesTest, expected_result) {
  ASSERT_EQ(expected_result, __curves_fixed_shr_rtz_s128(value, shift));
}

// shift 0: no truncation occurs
FixedShrRtzS128EdgeCasesTestParam shr_rtz_s128_shift_0[] = {
    // S128_MAX doesn't round down only when shift is 0
    {S128_MAX - 0, 0, ((S128_MAX - 0) >> 0) + 0},
    {S128_MAX - 1, 0, ((S128_MAX - 1) >> 0) + 0},

    // first positive boundary
    {(1LL << 0) + 1, 0, ((1LL << 0) + 1) >> 0},
    {(1LL << 0) + 0, 0, ((1LL << 0) + 0) >> 0},
    {(1LL << 0) - 1, 0, ((1LL << 0) - 1) >> 0},

    // boundary at zero
    {0LL + 1, 0, ((0LL + 1) >> 0) + 0},
    {0LL + 0, 0, ((0LL + 0) >> 0) + 0},
    {0LL - 1, 0, ((0LL - 1) >> 0) + 0},

    // first negative boundary
    {-(1LL << 0) + 1, 0, ((-(1LL << 0) + 1) >> 0) + 0},
    {-(1LL << 0) + 0, 0, ((-(1LL << 0) + 0) >> 0) + 0},
    {-(1LL << 0) - 1, 0, ((-(1LL << 0) - 1) >> 0) + 0},

    // boundary at min
    {S128_MIN + 1, 0, ((S128_MIN + 1) >> 0) + 0},
    {S128_MIN + 0, 0, ((S128_MIN + 0) >> 0) + 0},
};
INSTANTIATE_TEST_SUITE_P(shift_0, FixedShrRtzS128EdgeCasesTest,
                         ValuesIn(shr_rtz_s128_shift_0));

// shift 127: no positive integers, only one negative and it is the boundary
FixedShrRtzS128EdgeCasesTestParam shr_rtz_s128_shift_127[] = {
    // boundary at zero
    {+1, 127, (static_cast<s128>(+1) >> 127) + 0},  // rounds down
    {+0, 127, (static_cast<s128>(+0) >> 127) + 0},
    {-1, 127, (static_cast<s128>(-1) >> 127) + 1},  // rounds up

    // boundary at min
    {S128_MIN + 1, 127, ((S128_MIN + 1) >> 127) + 1},  // rounds up
    {S128_MIN + 0, 127, ((S128_MIN + 0) >> 127) + 0},
};
INSTANTIATE_TEST_SUITE_P(shift_127, FixedShrRtzS128EdgeCasesTest,
                         ValuesIn(shr_rtz_s128_shift_127));

}  // namespace
}  // namespace curves
