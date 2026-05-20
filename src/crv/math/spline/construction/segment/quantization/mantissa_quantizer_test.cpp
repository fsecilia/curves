// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "mantissa_quantizer.hpp"
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

using mantissa_t = int32_t;
constexpr auto quantize = mantissa_quantizer_t<mantissa_t>{};

// passthrough with no shift
static_assert(quantize(100, 0) == 100);

// basic shifting
static_assert(quantize(100, 2) == 25);
static_assert(quantize(-100, 2) == -25);

// rne boundary conditions
static_assert(quantize(3, 1) == 2); // 1.5 rounds up to 2
static_assert(quantize(5, 1) == 2); // 2.5 rounds down to 2
static_assert(quantize(7, 1) == 4); // 3.5 rounds up to 4
static_assert(quantize(-3, 1) == -2); // -1.5 rounds to -2
static_assert(quantize(-5, 1) == -2); // -2.5 rounds to -2

// container shift saturation; max_container_shift for int32_t is 31
static_assert(quantize(max<mantissa_t>(), 30) == (max<mantissa_t>() >> 30) + 1); // no flush before max
static_assert(quantize(max<mantissa_t>(), 31) == 0); // flush exactly at max
static_assert(quantize(max<mantissa_t>(), 32) == 0); // flush exceeding max
static_assert(quantize(max<mantissa_t>(), 100) == 0); // flush large values

} // namespace
} // namespace crv::spline
