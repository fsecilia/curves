// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "monotonicity.hpp"
#include <crv/math/fixed/fixed.hpp>
#include <crv/test/test.hpp>

namespace crv::spline::defect_checks {
namespace {

using sut_t = monotonicity_t;
using value_t = int64_t;
using coeff_t = fixed_t<value_t, 47>;

// returns true if a segment with these coefficients is monotonic
constexpr auto is_monotonic(value_t a, value_t b, value_t c) -> bool
{
    // the predicate returns true if monotonicity is violated and the interval should be refined, so invert it here
    return !sut_t{}(cubic_polynomial_t{coeff_t{a}, coeff_t{b}, coeff_t{c}, coeff_t{0}});
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

// wide-multiply boundary
//
// Narrow `3a` overflows `coeff_t` once `a` exceeds ~21845, but each individual coefficient still constructs cleanly.
// Here, we test that a narrow implementation would overflow, but the wide implementation does not. Inputs below have
// `c >= 0`, so we always reach (or pass through) the right-endpoint check.

// right-endpoint wide path: narrow `3a` overflows but wide `3a + 2b + c = 80000 >= 0` correctly continues;
// also exercises the vertex-bound wide path (narrow `-3a` would overflow) and the discriminant wide path
static_assert(is_monotonic(30000, -10000, 10000)); // `3a+2b+c = 80000`; vertex inside; `3ac = 9e8 >= b^2 = 1e8`

// right-endpoint wide path returns false: narrow `3a` overflows but wide correctly computes `3a + 2b + c = -10000 < 0`
static_assert(!is_monotonic(30000, -50000, 0));

// right-endpoint passes (32000 >= 0), then vertex-bound wide path is taken with narrow-overflowing `-3a`;
// vertex is inside (-90000 < -29000 < 0), discriminant rejects: `3ac = 0 < b^2 = 8.41e8`
static_assert(!is_monotonic(30000, -29000, 0));

} // namespace
} // namespace crv::spline::defect_checks
