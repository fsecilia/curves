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
    {1, static_cast<s128>(1) << 64},         // large multiplier
    {1, (static_cast<s128>(1) << 125) - 1},  // very large odd multiplier
    {1, static_cast<s128>(1) << 125},        // max scale for this shift
};
INSTANTIATE_TEST_SUITE_P(shift_1, FixedShrRtzS128CommonCasesTest,
                         ValuesIn(shr_rtz_s128_shift_1));

const FixedShrRtzS128CommonCasesTestParam shr_rtz_s128_shift_16[] = {
    {16, 1},                                  // unity
    {16, 2},                                  // smallest nonunity multiplier
    {16, 3},                                  // small odd multiplier
    {16, 1LL << 48},                          // large multiplier
    {16, (static_cast<s128>(1) << 111) - 1},  // max scale for this shift
};
INSTANTIATE_TEST_SUITE_P(shift_16, FixedShrRtzS128CommonCasesTest,
                         ValuesIn(shr_rtz_s128_shift_16));

const FixedShrRtzS128CommonCasesTestParam shr_rtz_s128_shift_32[] = {
    {32, 1},                                 // unity
    {32, 2},                                 // smallest nonunity multiplier
    {32, 3},                                 // small odd multiplier
    {32, (static_cast<s128>(1) << 48)},      // representative multiplier
    {32, (static_cast<s128>(1) << 95) - 1},  // max scale for this shift
};
INSTANTIATE_TEST_SUITE_P(shift_32, FixedShrRtzS128CommonCasesTest,
                         ValuesIn(shr_rtz_s128_shift_32));

const FixedShrRtzS128CommonCasesTestParam shr_rtz_s128_shift_64[] = {
    {64, 1},                                 // unity
    {64, 2},                                 // smallest nonunity multiplier
    {64, 3},                                 // small odd multiplier
    {64, (static_cast<s128>(1) << 32)},      // representative multiplier
    {64, (static_cast<s128>(1) << 63) - 1},  // max scale for this shift
};
INSTANTIATE_TEST_SUITE_P(shift_64, FixedShrRtzS128CommonCasesTest,
                         ValuesIn(shr_rtz_s128_shift_64));

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

// ----------------------------------------------------------------------------
// __curves_fixed_shl_sat_s128
// ----------------------------------------------------------------------------

struct FixedShlSatS128TestParam {
  s128 value;
  unsigned int shift;
  s128 expected_result;

  friend auto operator<<(std::ostream& out, const FixedShlSatS128TestParam& src)
      -> std::ostream& {
    return out << "{" << src.value << ", " << src.shift << ", "
               << src.expected_result << "}";
  }
};

struct FixedShlSatS128Test : TestWithParam<FixedShlSatS128TestParam> {
  s128 value = GetParam().value;
  unsigned int shift = GetParam().shift;
  s128 expected_result = GetParam().expected_result;
};

TEST_P(FixedShlSatS128Test, expected_result) {
  ASSERT_EQ(expected_result, __curves_fixed_shl_sat_s128(value, shift));
}

// Zero with various shifts always returns zero, regardless of shift amount.
const FixedShlSatS128TestParam shl_sat_s128_zero_with_various_shifts[] = {
    {0, 0, 0}, {0, 1, 0}, {0, 32, 0}, {0, 64, 0}, {0, 127, 0},
};
INSTANTIATE_TEST_SUITE_P(zero_with_various_shifts, FixedShlSatS128Test,
                         ValuesIn(shl_sat_s128_zero_with_various_shifts));

/*
  When shift is zero, the function returns the original value unchanged, since
  no shifting occurs and no overflow is possible.
*/
const FixedShlSatS128TestParam shl_sat_s128_shift_0[] = {
    {1, 0, 1},   {100, 0, 100},   {S128_MAX, 0, S128_MAX},
    {-1, 0, -1}, {-100, 0, -100}, {S128_MIN, 0, S128_MIN},
};
INSTANTIATE_TEST_SUITE_P(shift_0, FixedShlSatS128Test,
                         ValuesIn(shl_sat_s128_shift_0));

/*
  Small positive values that fit within the safe range and shift without
  overflow. These demonstrate normal operation where the result is simply
  value << shift.
*/
const FixedShlSatS128TestParam shl_sat_s128_normal_operation[] = {
    {1, 1, 2},
    {1, 10, 1 << 10},
    {1, 126, static_cast<s128>(1) << 126},
    {100, 10, 100 << 10},
    {1000, 20, 1000LL << 20},
};
INSTANTIATE_TEST_SUITE_P(normal_operation, FixedShlSatS128Test,
                         ValuesIn(shl_sat_s128_normal_operation));

/*
  Small negative values that shift safely. Negative values shift the same way
  as positive values, preserving the sign bit.
*/
const FixedShlSatS128TestParam shl_sat_s128_small_negatives[] = {
    {-1, 1, -2},
    {-1, 10, -(1 << 10)},
    {-1, 126, -(static_cast<s128>(1) << 126)},
    {-100, 10, -(100 << 10)},
    {-1000, 20, -(1000LL << 20)},
};
INSTANTIATE_TEST_SUITE_P(small_negatives, FixedShlSatS128Test,
                         ValuesIn(shl_sat_s128_small_negatives));

/*
  Mixed magnitude cases showing practical values and their behavior at
  different shift amounts. These verify the function works correctly for
  values commonly seen in real-world, fixed-point arithmetic.
*/
const FixedShlSatS128TestParam shl_sat_s128_mixed_magnitude[] = {
    {1000000, 40, static_cast<s128>(1000000) << 40},  // safe
    {1000000, 60, static_cast<s128>(1000000) << 60},  // Large but safe
    {1000000, 120, S128_MAX},  // Larger shift causes saturation
    {-1000000, 40, -(static_cast<s128>(1000000) << 40)},  // Negative safe
    {-1000000, 60,
     -(static_cast<s128>(1000000) << 60)},  // Negative large but safe
    {-1000000, 120, S128_MIN},  // Negative with large shift saturates
};
INSTANTIATE_TEST_SUITE_P(mixed_magnitude, FixedShlSatS128Test,
                         ValuesIn(shl_sat_s128_mixed_magnitude));

/*
  Boundary cases for shift == 1. The safe range is
  [S128_MIN >> 1, S128_MAX >> 1].
*/
const FixedShlSatS128TestParam shl_sat_s128_shift_1_boundaries[] = {
    // Positive saturation boundary.
    {S128_MAX >> 1, 1,
     (S128_MAX >> 1) << 1},              // Right at boundary, shifts safely
    {(S128_MAX >> 1) + 1, 1, S128_MAX},  // Just over boundary, saturates
    {S128_MAX, 1, S128_MAX},             // Far over boundary, saturates

    // Negative saturation boundary.
    {S128_MIN >> 1, 1, S128_MIN},        // Right at boundary, shifts safely
    {(S128_MIN >> 1) - 1, 1, S128_MIN},  // Just under boundary, saturates
    {S128_MIN, 1, S128_MIN},             // Far under boundary, saturates
};
INSTANTIATE_TEST_SUITE_P(shift_1_boundaries, FixedShlSatS128Test,
                         ValuesIn(shl_sat_s128_shift_1_boundaries));

/*
  Boundary cases for shift == 2. The safe range is
  [S128_MIN >> 2, S128_MAX >> 2].
*/
const FixedShlSatS128TestParam shl_sat_s128_shift_2_boundaries[] = {
    // Positive saturation cases.
    {S128_MAX >> 2, 2, (S128_MAX >> 2) << 2},  // At boundary, safe
    {(S128_MAX >> 2) + 1, 2, S128_MAX},        // Just over, saturates
    {S128_MAX, 2, S128_MAX},                   // Far over, saturates

    // Negative saturation cases.
    {S128_MIN >> 2, 2, S128_MIN},        // At boundary, safe
    {(S128_MIN >> 2) - 1, 2, S128_MIN},  // Just under, saturates
    {S128_MIN, 2, S128_MIN},             // Far under, saturates
};
INSTANTIATE_TEST_SUITE_P(shift_2_boundaries, FixedShlSatS128Test,
                         ValuesIn(shl_sat_s128_shift_2_boundaries));

// Boundary cases for shift == 64. The safe range is the int64 range.
const FixedShlSatS128TestParam shl_sat_s128_shift_64[] = {
    {1, 64, static_cast<s128>(1) << 64},              // Beginning of range
    {S64_MAX, 64, static_cast<s128>(S64_MAX) << 64},  // Positive boundary, safe
    {static_cast<s128>(S64_MAX) + 1, 64, S128_MAX},   // Just over, saturates

    {-1, 64, -(static_cast<s128>(1) << 64)},         // Beginning of range
    {S64_MIN, 64, S128_MIN},                         // Negative boundary, safe
    {static_cast<s128>(S64_MIN) - 1, 64, S128_MIN},  // Just under, saturates
};
INSTANTIATE_TEST_SUITE_P(shift_64, FixedShlSatS128Test,
                         ValuesIn(shl_sat_s128_shift_64));

// Final normal case where shift == 126. The safe range is [-2, 1].
const FixedShlSatS128TestParam shl_sat_s128_shift_126[] = {
    {1, 126, static_cast<s128>(1) << 126},  // At positive boundary, safe
    {2, 126, S128_MAX},                     // Over positive boundary, saturates
    {-1, 126, -(static_cast<s128>(1) << 126)},  // Safe negative value
    {-2, 126, S128_MIN},                        // At negative boundary, safe
    {-3, 126, S128_MIN},  // Under negative boundary, saturates
};
INSTANTIATE_TEST_SUITE_P(shift_126, FixedShlSatS128Test,
                         ValuesIn(shl_sat_s128_shift_126));

/*
  Maximum shift of 127 bits. The safe range becomes [-1, 0]. Only these two
  values can be shifted without saturation, but -1 << 127 is indistinguishable
  from saturation anyway.
*/
const FixedShlSatS128TestParam shl_sat_s128_shift_127[] = {
    {0, 127, 0},           // Only safe positive value
    {-1, 127, S128_MIN},   // Only safe negative value
    {1, 127, S128_MAX},    // Any positive value saturates
    {100, 127, S128_MAX},  // Large positive saturates
    {-2, 127, S128_MIN},   // Any value less than -1 saturates
};
INSTANTIATE_TEST_SUITE_P(shift_127, FixedShlSatS128Test,
                         ValuesIn(shl_sat_s128_shift_127));

// ----------------------------------------------------------------------------
// curves_fixed_rescale_s128
// ----------------------------------------------------------------------------

struct FixedRescaleS128TestParam {
  s128 value;
  unsigned int frac_bits;
  unsigned int output_frac_bits;
  s128 expected_result;

  friend auto operator<<(std::ostream& out,
                         const FixedRescaleS128TestParam& src)
      -> std::ostream& {
    return out << "{" << src.value << ", " << src.frac_bits << ", "
               << src.output_frac_bits << ", " << src.expected_result << "}";
  }
};

struct FixedRescaleS128Test : TestWithParam<FixedRescaleS128TestParam> {
  s128 value = GetParam().value;
  unsigned int frac_bits = GetParam().frac_bits;
  unsigned int output_frac_bits = GetParam().output_frac_bits;
  s128 expected_result = GetParam().expected_result;
};

TEST_P(FixedRescaleS128Test, expected_result) {
  ASSERT_EQ(expected_result,
            curves_fixed_rescale_s128(value, frac_bits, output_frac_bits));
}

// Tests that invalid scales are correctly dispatched to the error handler.
const FixedRescaleS128TestParam rescale_s128_invalid_scales[] = {
    // frac_bits >= 128, triggers error handler
    // output < frac, return 0
    {100, 128, 127, 0},

    // output_frac_bits >= 128, triggers error handler
    // value > 0, output >= frac, saturate max
    {1, 64, 128, S128_MAX},

    // both >= 128, triggers error handler)
    // value < 0, output >= frac, saturate min
    {-1, 128, 128, S128_MIN},
};
INSTANTIATE_TEST_SUITE_P(invalid_scales, FixedRescaleS128Test,
                         ValuesIn(rescale_s128_invalid_scales));

// Right shift path (output_frac_bits < frac_bits)
const FixedRescaleS128TestParam rescale_s128_right_shift[] = {
    // Basic positive with mid-range params
    {35LL << 32, 96, 64, 35},

    // Negative value
    {-35LL << 32, 96, 64, -35},

    // Zero
    {0, 96, 64, 0},

    // Boundary: frac_bits at 127 (maximum valid)
    {static_cast<s128>(100) << 63, 127, 64, 100},

    // Boundary: output_frac_bits at 0 (minimum valid)
    {static_cast<s128>(35) << 64, 64, 0, 35},

    // Large shift amount (shift by 120)
    {static_cast<s128>(3) << 120, 122, 2, 3},

    // Extreme value: S128_MAX (safe because right shift)
    {S128_MAX, 96, 64, S128_MAX >> 32},
};
INSTANTIATE_TEST_SUITE_P(right_shift, FixedRescaleS128Test,
                         ValuesIn(rescale_s128_right_shift));

// Equal path (output_frac_bits == frac_bits)
const FixedRescaleS128TestParam rescale_s128_no_shift[] = {
    // Basic positive
    {35LL << 32, 96, 96, 35LL << 32},

    // Zero
    {0, 96, 96, 0},

    // Boundary: both at 0 (minimum valid)
    {35, 0, 0, 35},

    // Boundary: both at 127 (maximum valid)
    {100, 127, 127, 100},

    // Extreme value: S128_MAX
    {S128_MAX, 96, 96, S128_MAX},
};
INSTANTIATE_TEST_SUITE_P(no_shift, FixedRescaleS128Test,
                         ValuesIn(rescale_s128_no_shift));

// Left shift path (output_frac_bits > frac_bits)
const FixedRescaleS128TestParam rescale_s128_left_shift[] = {
    // Basic positive with mid-range params
    {35, 64, 96, 35LL << 32},

    // Negative value
    {-35, 64, 96, -35LL << 32},

    // Zero
    {0, 64, 96, 0},

    // Boundary: output_frac_bits at 127
    {100, 64, 127, static_cast<s128>(100) << 63},

    // Large shift amount (shift by 120)
    {3, 0, 120, static_cast<s128>(3) << 120},
};
INSTANTIATE_TEST_SUITE_P(left_shift, FixedRescaleS128Test,
                         ValuesIn(rescale_s128_left_shift));

// Edge cases.
const FixedRescaleS128TestParam rescale_s128_edge_cases[] = {
    // Saturation: large positive that overflows -> S128_MAX
    // S128_MAX >> 4 shifted left by 5 overflows (bit 122 -> bit 127)
    {S128_MAX >> 4, 122, 127, S128_MAX},

    // Saturation: large negative that overflows -> S128_MIN
    // S128_MIN >> 4 shifted left by 5 overflows
    {S128_MIN >> 4, 122, 127, S128_MIN},

    // No overflow: large positive that fits
    // S128_MAX >> 10 shifted left by 10 fits exactly
    {S128_MAX >> 10, 117, 127, (S128_MAX >> 10) << 10},

    // No overflow: large negative that fits
    {S128_MIN >> 10, 117, 127, (S128_MIN >> 10) << 10},

    // Threshold: exactly at overflow boundary (positive)
    // Largest positive value with top 5 bits zero
    {(static_cast<s128>(1) << 122) - 1, 122, 127,
     ((static_cast<s128>(1) << 122) - 1) << 5},

    // Threshold: exactly at overflow boundary (negative)
    // Most negative value with top 5 bits as ones (sign extension)
    {-(static_cast<s128>(1) << 122), 122, 127,
     (-(static_cast<s128>(1) << 122)) << 5},
};
INSTANTIATE_TEST_SUITE_P(edge_cases, FixedRescaleS128Test,
                         ValuesIn(rescale_s128_edge_cases));

}  // namespace
}  // namespace curves
