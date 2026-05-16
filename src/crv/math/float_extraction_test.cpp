// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "float_extraction.hpp"
#include <crv/test/test.hpp>

namespace crv {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// float_extractor_t
// --------------------------------------------------------------------------------------------------------------------

namespace float_extraction_tests {

constexpr auto sut = float_extractor_t<float>{};

// 1.0f -> 1.0 * 2^0 -> mantissa: (1<<23), exp: -23
constexpr auto one = sut(1.0f);
static_assert(one.mantissa == 0x00800000);
static_assert(one.exponent == -23);

// 1.5f -> 1.5 * 2^0 -> mantissa: (1<<23) + (1<<22), exp: -23
constexpr auto one_point_five = sut(1.5f);
static_assert(one_point_five.mantissa == 0x00c00000);
static_assert(one_point_five.exponent == -23);

// -2.0f -> -1.0 * 2^1 -> mantissa: -(1<<23), exp: -22
constexpr auto negative_two = sut(-2.0f);
static_assert(negative_two.mantissa == -0x00800000);
static_assert(negative_two.exponent == -22);

// exact zero triggers ftz branch
constexpr auto zero = sut(0.0f);
static_assert(zero.mantissa == 0);
static_assert(zero.exponent == 0);

// negative zero also triggers ftz branch
constexpr auto negative_zero = sut(-0.0f);
static_assert(negative_zero.mantissa == 0);
static_assert(negative_zero.exponent == 0);

// subnormal/denormal triggers ftz branch
// 1.401298e-45f is the smallest positive subnormal for 32-bit floats
constexpr auto smallest_positive_subnormal_float = std::bit_cast<float>(0x00000001);
static_assert(smallest_positive_subnormal_float == 1.401298e-45f);
constexpr auto subnormal = sut(smallest_positive_subnormal_float);
static_assert(subnormal.mantissa == 0);
static_assert(subnormal.exponent == 0);

} // namespace float_extraction_tests

} // namespace
} // namespace crv
