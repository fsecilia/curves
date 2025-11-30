// SPDX-License-Identifier: MIT
/*!
  \file
  \brief isqrt testing facilities

  \copyright Copyright (C) 2025 Frank Secilia
*/

#pragma once

#include <curves/test/test.hpp>
#include <curves/fixed.hpp>

namespace curves {

struct isqrt_u64_test_vector {
  u64 x;
  unsigned int frac_bits;
  unsigned int output_frac_bits;
};

struct isqrt_u64_test_expected_result {
  isqrt_u64_test_vector test_vector;

  u64 y;
  u128 expected;
  u128 actual;
  u128 tolerance;
  u128 diff;
};

struct isqrt_u64_test_expected_result create_isqrt_u64_test_expected_result(
    struct isqrt_u64_test_vector test_vector);

void isqrt_u64_test_verify_result(
    struct isqrt_u64_test_expected_result expected_result);

void isqrt_u64_test_verify_test_vector(
    struct isqrt_u64_test_vector test_vector);

void isqrt_test_verify_u64(u64 x, unsigned int frac_bits,
                           unsigned int output_frac_bits);

}  // namespace curves
