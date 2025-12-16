// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <curves/testing/test.hpp>
#include <curves/io.hpp>

extern "C" {
#include <curves/driver/math.h>
}  // extern "C"

namespace curves {
namespace {

// ----------------------------------------------------------------------------
// curves_narrow_s128_s64
// ----------------------------------------------------------------------------

struct NarrowS128ToS64TestParam {
  s128 wide;
  s64 narrow;

  friend auto operator<<(std::ostream& out, const NarrowS128ToS64TestParam& src)
      -> std::ostream& {
    return out << "{" << src.wide << ", " << src.narrow << "}";
  }
};

struct NarrowS128ToS64Test : TestWithParam<NarrowS128ToS64TestParam> {};

TEST_P(NarrowS128ToS64Test, narrow) {
  const auto expected = GetParam().narrow;

  const auto actual = curves_narrow_s128_s64(GetParam().wide);

  ASSERT_EQ(expected, actual);
}

const NarrowS128ToS64TestParam narrow_tests[] = {
    {0, 0},                        // Zero stays zero
    {1, 1},                        // Small positive values pass through
    {-1, -1},                      // Small negative values pass through
    {S64_MAX, S64_MAX},            // Maximum 64-bit value passes through
    {S64_MIN, S64_MIN},            // Minimum 64-bit value passes through
    {(s128)S64_MAX + 1, S64_MAX},  // Just beyond max saturates
    {(s128)S64_MIN - 1, S64_MIN},  // Just beyond min saturates
    {CURVES_S128_MAX, S64_MAX},    // Far beyond max saturates
    {CURVES_S128_MIN, S64_MIN},    // Far beyond min saturates
};

INSTANTIATE_TEST_SUITE_P(narrow, NarrowS128ToS64Test, ValuesIn(narrow_tests));

// ----------------------------------------------------------------------------
// curves_int_log2()
// ----------------------------------------------------------------------------

struct IntLog2Param {
  u64 value;
  u64 expected_result;

  friend auto operator<<(std::ostream& out, const IntLog2Param& src)
      -> std::ostream& {
    return out << "{" << src.value << ", " << src.expected_result << " } ";
  }
};

struct IntLog2Test : TestWithParam<IntLog2Param> {};

TEST_P(IntLog2Test, expected_result) {
  const auto actual_result = curves_log2_u64(GetParam().value);

  ASSERT_EQ(GetParam().expected_result, actual_result);
}

// Covers all of int log2 using bva.
const IntLog2Param int_log2[] = {
    // Bottom valid range.
    {(1ULL << 0) + 0, 0},
    {(1ULL << 0) + 1, 1},

    {(1ULL << 1) - 1, 0},
    {(1ULL << 1) + 0, 1},
    {(1ULL << 1) + 1, 1},

    {(1ULL << 2) - 1, 1},
    {(1ULL << 2) + 0, 2},
    {(1ULL << 2) + 1, 2},

    {(1ULL << 3) - 1, 2},
    {(1ULL << 3) + 0, 3},
    {(1ULL << 3) + 1, 3},

    // Top of valid range.
    {(1ULL << 62) - 1, 61},
    {(1ULL << 62) + 0, 62},
    {(1ULL << 62) + 1, 62},

    {(1ULL << 63) - 1, 62},
    {(1ULL << 63) + 0, 63},
    {(1ULL << 63) + 1, 63},

    // Max boundary.
    {U64_MAX, 63},
};
INSTANTIATE_TEST_SUITE_P(all_cases, IntLog2Test, ValuesIn(int_log2));

}  // namespace
}  // namespace curves
