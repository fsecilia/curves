// SPDX-License-Identifier: MIT
/*!
  \file
  \copyright Copyright (C) 2025 Frank Secilia
*/

#include <curves/test.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

extern "C" {
#include <curves/driver/math64.h>
}  // extern "C"

#pragma GCC diagnostic pop

namespace curves {
namespace {

const auto kMin = std::numeric_limits<int64_t>::min();
const auto kMax = std::numeric_limits<int64_t>::max();

// ----------------------------------------------------------------------------
// curves_truncate_s64() Tests
// ----------------------------------------------------------------------------

struct CurvesTruncateS64TestParam {
  unsigned int frac_bits;
  s64 value;
  unsigned int shift;
  s64 expected_result;

  friend auto operator<<(std::ostream& out,
                         const CurvesTruncateS64TestParam& src)
      -> std::ostream& {
    return out << "{" << src.frac_bits << ", " << src.value << ", " << src.shift
               << ", " << src.expected_result << "}";
  }
};

struct CurvesTruncateS64Test : TestWithParam<CurvesTruncateS64TestParam> {};

TEST_P(CurvesTruncateS64Test, curves_truncate_s64) {
  ASSERT_EQ(GetParam().expected_result,
            __curves_truncate_s64(GetParam().frac_bits, GetParam().value,
                                  GetParam().shift));
}

CurvesTruncateS64TestParam truncate_s64_nonnegative_values[] = {
    // zero
    {0, 0, 0, 0},
    {0, 0, 1, 0},

    // single bit
    {0, 1, 0, 1},
    {0, 1, 1, 0},
    {0, 1, 63, 0},

    // multiple bits
    {0, 3, 0, 3},
    {0, 3, 1, 1},
    {0, 3, 2, 0},

    // boundary
    {0, kMax, 0, kMax},
    {0, kMax, 1, kMax >> 1},
    {0, kMax, 32, kMax >> 32},
    {0, kMax, 62, 1},
    {0, kMax, 63, 0},
};

INSTANTIATE_TEST_SUITE_P(nonnegative_values, CurvesTruncateS64Test,
                         ValuesIn(truncate_s64_nonnegative_values));

}  // namespace
}  // namespace curves
