// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia

  This file contains tests for s64 versions of functions that also have an s128
  version.
*/

#include <curves/test.hpp>
#include <curves/fixed.hpp>

namespace curves {
namespace {

// ----------------------------------------------------------------------------
// curves_saturate_s64
// ----------------------------------------------------------------------------

TEST(curves_saturate_s64, negative) {
  ASSERT_EQ(S64_MIN, curves_saturate_s64(false));
}

TEST(curves_saturate_s64, positive) {
  ASSERT_EQ(S64_MAX, curves_saturate_s64(true));
}

// ----------------------------------------------------------------------------
// __curves_fixed_rescale_error_s64
// ----------------------------------------------------------------------------

struct FixedRescaleErrorS64TestParam {
  s64 value;
  unsigned int frac_bits;
  unsigned int output_frac_bits;
  s64 expected_result;

  friend auto operator<<(std::ostream& out,
                         const FixedRescaleErrorS64TestParam& src)
      -> std::ostream& {
    return out << "{" << src.value << ", " << src.frac_bits << ", "
               << src.output_frac_bits << ", " << src.expected_result << "}";
  }
};

struct FixedRescaleErrorS64Test : TestWithParam<FixedRescaleErrorS64TestParam> {
};

TEST_P(FixedRescaleErrorS64Test, expected_result) {
  const auto actual_result = __curves_fixed_rescale_error_s64(
      GetParam().value, GetParam().frac_bits, GetParam().output_frac_bits);

  ASSERT_EQ(GetParam().expected_result, actual_result);
}

/*
  Tests zero-value inputs. These always return zero regardless of shift
  direction or precision, since zero can't overflow.
*/
const FixedRescaleErrorS64TestParam rescale_error_s64_zero_values[] = {
    {0, 0, 0, 0},  // All fractional bits zero
    {0, 1, 1, 0},  // No shift, nonzero fractional bits
    {0, 1, 0, 0},  // Right shift
    {0, 0, 1, 0},  // Left shift
};
INSTANTIATE_TEST_SUITE_P(zero_values, FixedRescaleErrorS64Test,
                         ValuesIn(rescale_error_s64_zero_values));

/*
  Tests right shift cases, output_frac_bits < frac_bits. The error handler
  returns zero for right shifts regardless of the input value, since right
  shifts reduce magnitude and cannot cause overflow.
*/
const FixedRescaleErrorS64TestParam rescale_error_s64_shr[] = {
    {-1, 1, 0, 0},
    {1, 1, 0, 0},
};
INSTANTIATE_TEST_SUITE_P(right_shifts, FixedRescaleErrorS64Test,
                         ValuesIn(rescale_error_s64_shr));

/*
  Tests no-shift cases, output_frac_bits == frac_bits, with non-zero values.
  When an invalid number of fractional bits cause the error handler to be
  called with no shift required, non-zero values saturate based on their sign.
*/
const FixedRescaleErrorS64TestParam rescale_error_s64_no_shift_sat[] = {
    {1, 0, 0, S64_MAX},   // Positive saturates to max
    {-1, 0, 0, S64_MIN},  // Negative saturates to min
};
INSTANTIATE_TEST_SUITE_P(no_shift_saturation, FixedRescaleErrorS64Test,
                         ValuesIn(rescale_error_s64_no_shift_sat));

/*
  Tests left shift cases, output_frac_bits > frac_bits, with non-zero values.
  Left shifts that trigger the error handler cause saturation based on sign.
  Tests include both regular values and boundary values at S64_MAX/S64_MIN.
*/
const FixedRescaleErrorS64TestParam rescale_error_s64_shl_sat[] = {
    {1, 0, 1, S64_MAX},        // Positive regular value
    {-1, 0, 1, S64_MIN},       // Negative regular value
    {S64_MAX, 0, 1, S64_MAX},  // Positive boundary value
    {S64_MIN, 0, 1, S64_MIN},  // Negative boundary value
};
INSTANTIATE_TEST_SUITE_P(left_shift_saturation, FixedRescaleErrorS64Test,
                         ValuesIn(rescale_error_s64_shl_sat));

// ----------------------------------------------------------------------------
// __curves_fixed_shr_rtz_s64
// ----------------------------------------------------------------------------

// Tests values -1, 0, 1 at various shifts.
struct FixedShrRtzS64NearZeroTest : TestWithParam<unsigned int> {
  unsigned int shift = GetParam();
};

// The first value before 0 should round up to zero.
TEST_P(FixedShrRtzS64NearZeroTest, predecessor_rounds_up_towards_zero) {
  s64 value = -1;  // -1/divisor
  s64 expected = 0;
  ASSERT_EQ(expected, __curves_fixed_shr_rtz_s64(value, shift));
}

// 0 is a multiple of divisor, so it should not round in either direction.
TEST_P(FixedShrRtzS64NearZeroTest, exact_stays_zero) {
  s64 value = 0;  // 0 exactly
  s64 expected = 0;
  ASSERT_EQ(expected, __curves_fixed_shr_rtz_s64(value, shift));
}

// The first value after 0 should round down.
TEST_P(FixedShrRtzS64NearZeroTest, successor_rounds_down_towards_zero) {
  s64 value = 1;  // 1/divisor
  s64 expected = 0;
  ASSERT_EQ(expected, __curves_fixed_shr_rtz_s64(value, shift));
}
INSTANTIATE_TEST_SUITE_P(various_shifts, FixedShrRtzS64NearZeroTest,
                         Values(1, 16, 32, 62, 63));

// ----------------------------------------------------------------------------

// Tests shifts and scales that aren't boundary conditions.
struct FixedShrRtzS64CommonCasesTestParam {
  unsigned int shift;
  s64 scale;

  friend auto operator<<(std::ostream& out,
                         const FixedShrRtzS64CommonCasesTestParam& src)
      -> std::ostream& {
    return out << "{" << src.shift << ", " << src.scale << "}";
  }
};

struct FixedShrRtzS64CommonCasesTest
    : TestWithParam<FixedShrRtzS64CommonCasesTestParam> {
  unsigned int shift = GetParam().shift;
  s64 scale = GetParam().scale;
  s64 divisor = 1LL << shift;
};

// The first value before a negative multiple of divisor should round up.
TEST_P(FixedShrRtzS64CommonCasesTest,
       negative_predecessor_rounds_up_towards_zero) {
  const auto value = -scale * divisor - 1;  // -scale - 1/divisor
  const auto expected = -scale;

  const auto actual = __curves_fixed_shr_rtz_s64(value, shift);

  ASSERT_EQ(expected, actual);
}

// Exact multiples shouldn't round; there's no fractional part to handle.
TEST_P(FixedShrRtzS64CommonCasesTest, negative_exact_multiple_no_rounding) {
  s64 value = -scale * divisor;  // -scale exactly
  s64 expected = -scale;
  ASSERT_EQ(expected, __curves_fixed_shr_rtz_s64(value, shift));
}

// The first value after a negative multiple of divisor should round up.
TEST_P(FixedShrRtzS64CommonCasesTest,
       negative_successor_rounds_up_towards_zero) {
  s64 value = -scale * divisor + 1;  // -scale + 1/divisor
  s64 expected = -scale + 1;
  ASSERT_EQ(expected, __curves_fixed_shr_rtz_s64(value, shift));
}

// The first value before a positive multiple of divisor should round down.
TEST_P(FixedShrRtzS64CommonCasesTest,
       positive_predecessor_rounds_down_towards_zero) {
  const auto value = scale * divisor - 1;  // scale - 1/divisor
  const auto expected = scale - 1;

  const auto actual = __curves_fixed_shr_rtz_s64(value, shift);

  ASSERT_EQ(expected, actual);
}

// Exact multiples shouldn't round; there's no fractional part to handle.
TEST_P(FixedShrRtzS64CommonCasesTest, positive_exact_multiple_no_rounding) {
  s64 value = scale * divisor;  // scale exactly
  s64 expected = scale;
  ASSERT_EQ(expected, __curves_fixed_shr_rtz_s64(value, shift));
}

// The first value after a positive multiple of divisor should round down.
TEST_P(FixedShrRtzS64CommonCasesTest,
       positive_successor_rounds_down_towards_zero) {
  s64 value = scale * divisor + 1;  // scale + 1/divisor
  s64 expected = scale;
  ASSERT_EQ(expected, __curves_fixed_shr_rtz_s64(value, shift));
}

const FixedShrRtzS64CommonCasesTestParam shr_rtz_s64_shift_1[] = {
    {1, 1},                // unity
    {1, 2},                // smallest nonunity multiplier
    {1, 3},                // small odd multiplier
    {1, 1LL << 32},        // large multiplier
    {1, (1LL << 61) - 1},  // very large odd multiplier
    {1, 1LL << 61},        // max scale for this shift
};
INSTANTIATE_TEST_SUITE_P(shift_1, FixedShrRtzS64CommonCasesTest,
                         ValuesIn(shr_rtz_s64_shift_1));

const FixedShrRtzS64CommonCasesTestParam shr_rtz_s64_shift_16[] = {
    {16, 1},                // unity
    {16, 2},                // smallest nonunity multiplier
    {16, 3},                // small odd multiplier
    {16, 1LL << 24},        // large multiplier
    {16, (1LL << 47) - 1},  // max scale for this shift
};
INSTANTIATE_TEST_SUITE_P(shift_16, FixedShrRtzS64CommonCasesTest,
                         ValuesIn(shr_rtz_s64_shift_16));

const FixedShrRtzS64CommonCasesTestParam shr_rtz_s64_shift_32[] = {
    {32, 1},                // unity
    {32, 2},                // smallest nonunity multiplier
    {32, 3},                // small odd multiplier
    {32, (1LL << 16)},      // representative multiplier
    {32, (1LL << 31) - 1},  // max scale for this shift
};
INSTANTIATE_TEST_SUITE_P(shift_32, FixedShrRtzS64CommonCasesTest,
                         ValuesIn(shr_rtz_s64_shift_32));

const FixedShrRtzS64CommonCasesTestParam shr_rtz_s64_shift_62[] = {
    {62, 1},  // 62 has no room for scales
};
INSTANTIATE_TEST_SUITE_P(shift_62, FixedShrRtzS64CommonCasesTest,
                         ValuesIn(shr_rtz_s64_shift_62));

// ----------------------------------------------------------------------------

// Tests specific edge cases.
struct FixedShrRtzS64EdgeCasesTestParam {
  s64 value;
  unsigned int shift;
  s64 expected_result;

  friend auto operator<<(std::ostream& out,
                         const FixedShrRtzS64EdgeCasesTestParam& src)
      -> std::ostream& {
    return out << "{" << src.value << ", " << src.shift << ", "
               << src.expected_result << "}";
  }
};

struct FixedShrRtzS64EdgeCasesTest
    : TestWithParam<FixedShrRtzS64EdgeCasesTestParam> {
  s64 value = GetParam().value;
  unsigned int shift = GetParam().shift;
  s64 expected_result = GetParam().expected_result;
};

TEST_P(FixedShrRtzS64EdgeCasesTest, expected_result) {
  ASSERT_EQ(expected_result, __curves_fixed_shr_rtz_s64(value, shift));
}

// shift 0: no truncation occurs
FixedShrRtzS64EdgeCasesTestParam shr_rtz_s64_shift_0[] = {
    // S64_MAX doesn't round down only when shift is 0
    {S64_MAX + 0, 0, S64_MAX + 0},
    {S64_MAX - 1, 0, S64_MAX - 1},

    // first positive boundary
    {1 + 1, 0, 1 + 1},
    {1 + 0, 0, 1 + 0},
    {1 - 1, 0, 1 - 1},

    // boundary at zero
    {0 + 1, 0, 0 + 1},
    {0 + 0, 0, 0 + 0},
    {0 - 1, 0, 0 - 1},

    // first negative boundary
    {-1 + 1, 0, -1 + 1},
    {-1 + 0, 0, -1 + 0},
    {-1 - 1, 0, -1 - 1},

    // boundary at min
    {S64_MIN + 1, 0, S64_MIN + 1},
    {S64_MIN + 0, 0, S64_MIN + 0},
};
INSTANTIATE_TEST_SUITE_P(shift_0, FixedShrRtzS64EdgeCasesTest,
                         ValuesIn(shr_rtz_s64_shift_0));

// shift 63: no positive integers, only one negative and it is the boundary
FixedShrRtzS64EdgeCasesTestParam shr_rtz_s64_shift_63[] = {
    // boundary at zero
    {+1, 63, (+1LL >> 63) + 0},  // rounds down
    {+0, 63, (+0LL >> 63) + 0},
    {-1, 63, (-1LL >> 63) + 1},  // rounds up

    // boundary at min
    {S64_MIN + 1, 63, ((S64_MIN + 1) >> 63) + 1},  // rounds up
    {S64_MIN + 0, 63, ((S64_MIN + 0) >> 63) + 0},
};
INSTANTIATE_TEST_SUITE_P(shift_63, FixedShrRtzS64EdgeCasesTest,
                         ValuesIn(shr_rtz_s64_shift_63));

// ----------------------------------------------------------------------------
// __curves_fixed_shl_sat_s64
// ----------------------------------------------------------------------------

struct FixedShlSatS64TestParam {
  s64 value;
  unsigned int shift;
  s64 expected_result;

  friend auto operator<<(std::ostream& out, const FixedShlSatS64TestParam& src)
      -> std::ostream& {
    return out << "{" << src.value << ", " << src.shift << ", "
               << src.expected_result << "}";
  }
};

struct FixedShlSatS64Test : TestWithParam<FixedShlSatS64TestParam> {
  s64 value = GetParam().value;
  unsigned int shift = GetParam().shift;
  s64 expected_result = GetParam().expected_result;
};

TEST_P(FixedShlSatS64Test, expected_result) {
  ASSERT_EQ(expected_result, __curves_fixed_shl_sat_s64(value, shift));
}

// Zero with various shifts always returns zero, regardless of shift amount.
const FixedShlSatS64TestParam shl_sat_s64_zero_with_various_shifts[] = {
    {0, 0, 0},
    {0, 1, 0},
    {0, 32, 0},
    {0, 63, 0},
};
INSTANTIATE_TEST_SUITE_P(zero_with_various_shifts, FixedShlSatS64Test,
                         ValuesIn(shl_sat_s64_zero_with_various_shifts));

/*
  When shift is zero, the function returns the original value unchanged, since
  no shifting occurs and no overflow is possible.
*/
const FixedShlSatS64TestParam shl_sat_s64_shift_0[] = {
    {1, 0, 1},   {100, 0, 100},   {S64_MAX, 0, S64_MAX},
    {-1, 0, -1}, {-100, 0, -100}, {S64_MIN, 0, S64_MIN},
};
INSTANTIATE_TEST_SUITE_P(shift_0, FixedShlSatS64Test,
                         ValuesIn(shl_sat_s64_shift_0));

/*
  Small positive values that fit within the safe range and shift without
  overflow. These demonstrate normal operation where the result is simply
  value << shift.
*/
const FixedShlSatS64TestParam shl_sat_s64_normal_operation[] = {
    {1, 1, 2},
    {1, 10, 1 << 10},
    {1, 62, 1LL << 62},
    {100, 10, 100 << 10},
    {1000, 20, 1000LL << 20},
};
INSTANTIATE_TEST_SUITE_P(normal_operation, FixedShlSatS64Test,
                         ValuesIn(shl_sat_s64_normal_operation));

/*
  Small negative values that shift safely. Negative values shift the same way
  as positive values, preserving the sign bit.
*/
const FixedShlSatS64TestParam shl_sat_s64_small_negatives[] = {
    {-1, 1, -2},
    {-1, 10, -(1 << 10)},
    {-1, 62, -(1LL << 62)},
    {-100, 10, -(100 << 10)},
    {-1000, 20, -(1000LL << 20)},
};
INSTANTIATE_TEST_SUITE_P(small_negatives, FixedShlSatS64Test,
                         ValuesIn(shl_sat_s64_small_negatives));

/*
  Mixed magnitude cases showing practical values and their behavior at
  different shift amounts. These verify the function works correctly for
  values commonly seen in real-world, fixed-point arithmetic.
*/
const FixedShlSatS64TestParam shl_sat_s64_mixed_magnitude[] = {
    {1000000, 15, 1000000LL << 15},      // Large
    {1000000, 30, 1000000LL << 30},      // Large but safe
    {1000000, 60, S64_MAX},              // Larger shift causes saturation
    {-1000000, 15, -(1000000LL << 15)},  // Negative large
    {-1000000, 30, -(1000000LL << 30)},  // Negative large but safe
    {-1000000, 60, S64_MIN},             // Negative with large shift saturates
};
INSTANTIATE_TEST_SUITE_P(mixed_magnitude, FixedShlSatS64Test,
                         ValuesIn(shl_sat_s64_mixed_magnitude));

/*
  Boundary cases for shift == 1. The safe range is
  [S64_MIN >> 1, S64_MAX >> 1].
*/
const FixedShlSatS64TestParam shl_sat_s64_shift_1_boundaries[] = {
    // Positive saturation boundary.
    {S64_MAX >> 1, 1, (S64_MAX >> 1) << 1},  // Right at boundary, shifts safely
    {(S64_MAX >> 1) + 1, 1, S64_MAX},        // Just over boundary, saturates
    {S64_MAX, 1, S64_MAX},                   // Far over boundary, saturates

    // Negative saturation boundary.
    {S64_MIN >> 1, 1, S64_MIN},        // Right at boundary, shifts safely
    {(S64_MIN >> 1) - 1, 1, S64_MIN},  // Just under boundary, saturates
    {S64_MIN, 1, S64_MIN},             // Far under boundary, saturates
};
INSTANTIATE_TEST_SUITE_P(shift_1_boundaries, FixedShlSatS64Test,
                         ValuesIn(shl_sat_s64_shift_1_boundaries));

/*
  Boundary cases for shift == 2. The safe range is
  [S64_MIN >> 2, S64_MAX >> 2].
*/
const FixedShlSatS64TestParam shl_sat_s64_shift_2_boundaries[] = {
    // Positive saturation cases.
    {S64_MAX >> 2, 2, (S64_MAX >> 2) << 2},  // At boundary, safe
    {(S64_MAX >> 2) + 1, 2, S64_MAX},        // Just over, saturates
    {S64_MAX, 2, S64_MAX},                   // Far over, saturates

    // Negative saturation cases.
    {S64_MIN >> 2, 2, S64_MIN},        // At boundary, safe
    {(S64_MIN >> 2) - 1, 2, S64_MIN},  // Just under, saturates
    {S64_MIN, 2, S64_MIN},             // Far under, saturates
};
INSTANTIATE_TEST_SUITE_P(shift_2_boundaries, FixedShlSatS64Test,
                         ValuesIn(shl_sat_s64_shift_2_boundaries));

// Boundary cases for shift == 32. The safe range is the int32 range.
const FixedShlSatS64TestParam shl_sat_s64_shift_32[] = {
    {1, 32, 1LL << 32},                              // Beginning of range
    {S32_MAX, 32, static_cast<s64>(S32_MAX) << 32},  // Positive boundary, safe
    {static_cast<s64>(S32_MAX) + 1, 32, S64_MAX},    // Just over, saturates

    {-1, 32, -(1LL << 32)},                        // Beginning of range
    {S32_MIN, 32, S64_MIN},                        // Negative boundary, safe
    {static_cast<s64>(S32_MIN) - 1, 32, S64_MIN},  // Just under, saturates
};
INSTANTIATE_TEST_SUITE_P(shift_32, FixedShlSatS64Test,
                         ValuesIn(shl_sat_s64_shift_32));

// Final normal case where shift == 62. The safe range is [-2, 1].
const FixedShlSatS64TestParam shl_sat_s64_shift_62[] = {
    {1, 62, 1LL << 62},      // At positive boundary, safe
    {2, 62, S64_MAX},        // Over positive boundary, saturates
    {-1, 62, -(1LL << 62)},  // Safe negative value
    {-2, 62, S64_MIN},       // At negative boundary, safe
    {-3, 62, S64_MIN},       // Under negative boundary, saturates
};
INSTANTIATE_TEST_SUITE_P(shift_62, FixedShlSatS64Test,
                         ValuesIn(shl_sat_s64_shift_62));

/*
  Maximum shift of 63 bits. The safe range becomes [-1, 0]. Only these two
  values can be shifted without saturation, but -1 << 63 is indistinguishable
  from saturation anyway.
*/
const FixedShlSatS64TestParam shl_sat_s64_shift_63[] = {
    {0, 63, 0},          // Only safe positive value
    {-1, 63, S64_MIN},   // Only safe negative value
    {1, 63, S64_MAX},    // Any positive value saturates
    {100, 63, S64_MAX},  // Large positive saturates
    {-2, 63, S64_MIN},   // Any value less than -1 saturates
};
INSTANTIATE_TEST_SUITE_P(shift_63, FixedShlSatS64Test,
                         ValuesIn(shl_sat_s64_shift_63));

// ----------------------------------------------------------------------------
// curves_fixed_rescale_s64
// ----------------------------------------------------------------------------

struct FixedRescaleS64TestParam {
  s64 value;
  unsigned int frac_bits;
  unsigned int output_frac_bits;
  s64 expected_result;

  friend auto operator<<(std::ostream& out, const FixedRescaleS64TestParam& src)
      -> std::ostream& {
    return out << "{" << src.value << ", " << src.frac_bits << ", "
               << src.output_frac_bits << ", " << src.expected_result << "}";
  }
};

struct FixedRescaleS64Test : TestWithParam<FixedRescaleS64TestParam> {
  s64 value = GetParam().value;
  unsigned int frac_bits = GetParam().frac_bits;
  unsigned int output_frac_bits = GetParam().output_frac_bits;
  s64 expected_result = GetParam().expected_result;
};

TEST_P(FixedRescaleS64Test, expected_result) {
  ASSERT_EQ(expected_result,
            curves_fixed_rescale_s64(value, frac_bits, output_frac_bits));
}

// Tests that invalid scales are correctly dispatched to the error handler.
const FixedRescaleS64TestParam rescale_s64_invalid_scales[] = {
    // frac_bits >= 64, triggers error handler
    // output < frac, return 0
    {100, 64, 63, 0},

    // output_frac_bits >= 64, triggers error handler
    // value > 0, output >= frac, saturate max
    {1, 32, 64, S64_MAX},

    // both >= 64, triggers error handler)
    // value < 0, output >= frac, saturate min
    {-1, 64, 64, S64_MIN},
};
INSTANTIATE_TEST_SUITE_P(invalid_scales, FixedRescaleS64Test,
                         ValuesIn(rescale_s64_invalid_scales));

// Right shift path (output_frac_bits < frac_bits)
const FixedRescaleS64TestParam rescale_s64_right_shift[] = {
    // Basic positive with mid-range params
    {35LL << 16, 48, 32, 35},

    // Negative value
    {-35LL << 16, 48, 32, -35},

    // Zero
    {0, 48, 32, 0},

    // Boundary: frac_bits at 63 (maximum valid)
    {100LL << 31, 63, 32, 100},

    // Boundary: output_frac_bits at 0 (minimum valid)
    {35LL << 32, 32, 0, 35},

    // Large shift amount (shift by 60)
    {3LL << 60, 62, 2, 3},

    // Extreme value: S64_MAX (safe because right shift)
    {S64_MAX, 48, 32, (S64_MAX >> 16) + 1},
};
INSTANTIATE_TEST_SUITE_P(right_shift, FixedRescaleS64Test,
                         ValuesIn(rescale_s64_right_shift));

// Equal path (output_frac_bits == frac_bits)
const FixedRescaleS64TestParam rescale_s64_no_shift[] = {
    // Basic positive
    {35LL << 16, 40, 40, 35LL << 16},

    // Zero
    {0, 40, 40, 0},

    // Boundary: both at 0 (minimum valid)
    {35, 0, 0, 35},

    // Boundary: both at 63 (maximum valid)
    {100, 63, 63, 100},

    // Extreme value: S64_MAX
    {S64_MAX, 40, 40, S64_MAX},
};
INSTANTIATE_TEST_SUITE_P(no_shift, FixedRescaleS64Test,
                         ValuesIn(rescale_s64_no_shift));

// Left shift path (output_frac_bits > frac_bits)
const FixedRescaleS64TestParam rescale_s64_left_shift[] = {
    // Basic positive with mid-range params
    {35, 32, 48, 35LL << 16},

    // Negative value
    {-35, 32, 48, -35LL << 16},

    // Zero
    {0, 32, 48, 0},

    // Boundary: output_frac_bits at 63
    {100, 32, 63, 100LL << 31},

    // Large shift amount (shift by 60)
    {3, 0, 60, 3LL << 60},
};
INSTANTIATE_TEST_SUITE_P(left_shift, FixedRescaleS64Test,
                         ValuesIn(rescale_s64_left_shift));

// Edge cases.
const FixedRescaleS64TestParam rescale_s64_edge_cases[] = {
    // Saturation: large positive that overflows -> S64_MAX
    // S64_MAX >> 4 shifted left by 5 overflows (bit 58 -> bit 63)
    {S64_MAX >> 4, 58, 63, S64_MAX},

    // Saturation: large negative that overflows -> S64_MIN
    // S64_MIN >> 4 shifted left by 5 overflows
    {S64_MIN >> 4, 58, 63, S64_MIN},

    // No overflow: large positive that fits
    // S64_MAX >> 10 shifted left by 10 fits exactly
    {S64_MAX >> 10, 53, 63, (S64_MAX >> 10) << 10},

    // No overflow: large negative that fits
    {S64_MIN >> 10, 53, 63, (S64_MIN >> 10) << 10},

    // Threshold: exactly at overflow boundary (positive)
    // Largest positive value with top 5 bits zero
    {(1LL << 58) - 1, 58, 63, ((1LL << 58) - 1) << 5},

    // Threshold: exactly at overflow boundary (negative)
    // Most negative value with top 5 bits as ones (sign extension)
    {-(1LL << 58), 58, 63, (-(1LL << 58)) << 5},
};
INSTANTIATE_TEST_SUITE_P(edge_cases, FixedRescaleS64Test,
                         ValuesIn(rescale_s64_edge_cases));

}  // namespace
}  // namespace curves
