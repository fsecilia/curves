// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <curves/test.hpp>
#include <curves/io.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

extern "C" {
#include <curves/driver/math.h>
}  // extern "C"

#pragma GCC diagnostic pop

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

}  // namespace
}  // namespace curves
