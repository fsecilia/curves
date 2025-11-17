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

// ----------------------------------------------------------------------------
// curves_saturate_s64
// ----------------------------------------------------------------------------

TEST(curves_saturate_s64, negative) {
  ASSERT_EQ(S64_MIN, curves_saturate_s64(false));
}

TEST(curves_saturate_s64, positive) {
  ASSERT_EQ(S64_MAX, curves_saturate_s64(true));
}

TEST(curves_saturate_s128, negative) {
  ASSERT_EQ(CURVES_S128_MIN, curves_saturate_s128(false));
}

TEST(curves_saturate_s128, positive) {
  ASSERT_EQ(CURVES_S128_MAX, curves_saturate_s128(true));
}

}  // namespace
}  // namespace curves
