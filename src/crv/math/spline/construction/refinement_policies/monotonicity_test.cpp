// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "monotonicity.hpp"
#include <crv/math/fixed/fixed.hpp>
#include <crv/test/test.hpp>
#include <array>

namespace crv::spline::refinement_policies {
namespace {

using sut_t = monotonicity_t;
using value_t = int64_t;
using coeff_t = fixed_t<value_t, 47>;

// returns true if a segment with these coefficients is monotonic
constexpr auto is_monotonic(value_t a, value_t b, value_t c) -> bool
{
    return sut_t{}(coeff_t{a}, coeff_t{b}, coeff_t{c});
}

// c < 0
// left endpoint (t=0) has a negative slope
static_assert(!is_monotonic(1, 1, -1));

// 3a + 2b + c < 0
// right endpoint (t=1) has a negative slope
// c >= 0 is met, but the curve dips down by the end
static_assert(!is_monotonic(1, -3, 1));

// a <= 0
// derivative is a downward-opening parabola or a line
// endpoints are already known to be >= 0, the minimum must be at an endpoint
static_assert(is_monotonic(0, 1, 1)); // linear derivative, quadratic spline
static_assert(is_monotonic(-1, 0, 5)); // downward-opening parabola

// b <= -3a || 0 <= b
// parabola opens upward, but the vertex, the minimum slope, is entirely outside the (0, 1) interval
static_assert(is_monotonic(1, 0, 1)); // vertex is exactly at t=0
static_assert(is_monotonic(1, 1, 1)); // vertex is at t < 0
static_assert(is_monotonic(1, -3, 4)); // vertex is exactly at t=1
static_assert(is_monotonic(1, -4, 6)); // vertex is at t > 1

// 3ac >= b^2
// vertex is inside (0, 1); slope dips in middle of the segment
// minimum slope at the vertex is >= 0, so it remains monotonic
static_assert(is_monotonic(1, -2, 2)); // 3(1)(2) = 6 >= (-2)^2 = 4, minimum slope is strictly positive
static_assert(is_monotonic(3, -3, 1)); // 3(3)(1) = 9 == (-3)^2 = 9, BVA: slope touches exactly zero at the vertex

// 3ac < b^2
// vertex is inside (0, 1); slope dips below zero
static_assert(!is_monotonic(1, -2, 1)); // 3(1)(1) = 3 < (-2)^2 = 4

// wide fixed-point boundary
// use large values that would overflow standard 64-bit bounds if not for wide multiplication
constexpr auto large_val = value_t{1} << 20;
static_assert(is_monotonic(large_val, -large_val, large_val));

} // namespace
} // namespace crv::spline::refinement_policies
