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

namespace float32 {

constexpr auto sut = float_extractor_t<float32_t>{};
using scaled_int_t = scaled_int_t<int32_t>;

// 1.0f -> 1.0 * 2^0 -> mantissa: (1<<23), exp: -23
static_assert(sut(1.0f) == scaled_int_t{0x00800000, -23});

// 1.5f -> 1.5 * 2^0 -> mantissa: (1<<23) + (1<<22), exp: -23
static_assert(sut(1.5f) == scaled_int_t{0x00c00000, -23});

// -2.0f -> -1.0 * 2^1 -> mantissa: -(1<<23), exp: -22
static_assert(sut(-2.0f) == scaled_int_t{-0x00800000, -22});

// exact zero triggers ftz branch
static_assert(sut(0.0f) == scaled_int_t{});
static_assert(sut(-0.0f) == scaled_int_t{});

// subnormal triggers ftz branch
constexpr auto smallest_positive_subnormal_float = std::bit_cast<float32_t>(0x00000001); // 2^-149 ~= 1.401298e-45
static_assert(sut(smallest_positive_subnormal_float) == scaled_int_t{});
static_assert(sut(-smallest_positive_subnormal_float) == scaled_int_t{});
constexpr auto largest_positive_subnormal_float = std::bit_cast<float32_t>(0x007FFFFF); // 2^-126 ~= 1.17549421e-38
static_assert(sut(largest_positive_subnormal_float) == scaled_int_t{});
static_assert(sut(-largest_positive_subnormal_float) == scaled_int_t{});
constexpr auto smallest_positive_normalized_float = std::bit_cast<float32_t>(0x00800000); // ~1.17549435e10^-38
static_assert(sut(smallest_positive_normalized_float) != scaled_int_t{});
static_assert(sut(-smallest_positive_normalized_float) != scaled_int_t{});

// max positive float
static_assert(sut(std::numeric_limits<float>::max()) == scaled_int_t{0x00FFFFFF, 104});

// max negative float
static_assert(sut(std::numeric_limits<float>::lowest()) == scaled_int_t{-0x00FFFFFF, 104});

} // namespace float32

static_assert(float_extractor_t<float64_t>{}(1.0) == scaled_int_t<int64_t>{int64_t{1} << 52, -52});

} // namespace float_extraction_tests

// --------------------------------------------------------------------------------------------------------------------
// exponent_renormalizer_t
// --------------------------------------------------------------------------------------------------------------------

namespace exponent_renormalizer_tests {

// arbitrary clamp bounds for testing
constexpr auto sut = exponent_renormalizer_t<-20, 20>{};

// pulling the 1.0f result from above: mantissa = 0x00800000, exponent = -23
constexpr auto src = scaled_int_t<int_t>{.mantissa = 0x00800000, .exponent = -23};

// clamping to -20 means shifting left by (-23 - (-20)) = -3, right-shifting the mantissa by 3
constexpr auto clamped = sut(src);
static_assert(clamped.exponent == -20);
static_assert(clamped.mantissa == 0x00100000); // 0x00800000 >> 3 == 0x00100000

} // namespace exponent_renormalizer_tests

} // namespace
} // namespace crv
