// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment.hpp"
#include <crv/math/fixed/io.hpp>
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

using coeff_value_t = int64_t;
using coeff_t = fixed_t<coeff_value_t, 48>;

using x_value_t = int64_t;
using x_t = fixed_t<x_value_t, 48>;
using y_t = x_t;

using sut_t = segment_t<x_t, y_t, coeff_t>;
using coeffs_t = sut_t::coeffs_t;
using vals_t = std::array<coeff_t::value_t, sut_t::coeff_count>;

constexpr auto c0 = coeff_t{0};
constexpr auto c1 = coeff_t{1};
constexpr auto c_max = max<coeff_value_t>();
constexpr auto c_min = min<coeff_value_t>();

// 56-bit bounds for C0 since it also packs log2_width
constexpr auto c0_max = coeff_value_t{c_max >> 8};
constexpr auto c0_min = coeff_value_t{c_min >> 8};

constexpr auto t0 = x_t::literal(0);
constexpr auto t_half = x_t::literal(1LL << (x_t::frac_bits - 1));
constexpr auto t_quarter = x_t::literal(1LL << (x_t::frac_bits - 2));
constexpr auto t_three_quarter = t_half + t_quarter;
constexpr auto t_max = x_t::literal((1LL << x_t::frac_bits) - 1);

constexpr auto coeffs = coeffs_t{};

// --------------------------------------------------------------------------------------------------------------------
// normalization shift
// --------------------------------------------------------------------------------------------------------------------

namespace normalization_shift_tests {

// fma that bypasses the math to just expose parameter t
struct passthrough_fma_t
{
    template <is_fixed y_t>
    [[nodiscard]] constexpr auto operator()(y_t, is_fixed auto t, is_fixed auto) const noexcept -> y_t
    {
        return y_t::convert(t);
    }
};

// zero shift
constexpr auto dx_quarter = x_t::literal(1LL << (x_t::frac_bits - 2));
constexpr auto sut0 = segment_t<x_t, coeff_t, coeff_t, passthrough_fma_t>({c0, c0, c0, c0}, 0);
static_assert(sut0.evaluate(dx_quarter).value == dx_quarter.value);

// positive shift (left shift dx, segment is narrower than 1.0)
// dx is 1/8, shift is 2 (multiply by 4), resulting t should be 0.5
constexpr auto dx_eighth = x_t::literal(1LL << (x_t::frac_bits - 3));
constexpr auto sut_left = segment_t<x_t, coeff_t, coeff_t, passthrough_fma_t>({c0, c0, c0, c0}, -2);
static_assert(sut_left.evaluate(dx_eighth).value == (dx_eighth.value << 2));

// negative shift (right shift dx, segment is wider than 1.0)
// dx is 2.0, shift is -2 (divide by 4), resulting t should be 0.5
constexpr auto dx_two = x_t::literal(2ULL << x_t::frac_bits);
constexpr auto sut_right = segment_t<x_t, coeff_t, coeff_t, passthrough_fma_t>({c0, c0, c0, c0}, 2);
static_assert(sut_right.evaluate(dx_two).value == (dx_two.value >> 2));

} // namespace normalization_shift_tests

// --------------------------------------------------------------------------------------------------------------------
// polynomial evaluation
// --------------------------------------------------------------------------------------------------------------------

constexpr auto evaluate(coeffs_t const& coeffs, int8_t log2_width, x_t dx) noexcept -> typename coeff_t::value_t
{
    return sut_t{coeffs, log2_width}.evaluate(dx).value;
}

constexpr auto evaluate(vals_t const& coeff_vals, int8_t log2_width, x_t dx) noexcept -> typename coeff_t::value_t
{
    return evaluate(coeffs_t{coeff_t::literal(coeff_vals[0]), coeff_t::literal(coeff_vals[1]),
                        coeff_t::literal(coeff_vals[2]), coeff_t::literal(coeff_vals[3])},
        log2_width, dx);
}

namespace evaluation_tests {

// all zeros
static_assert(evaluate({0, 0, 0, 0}, 0, t_half) == 0);

// early coeff
static_assert(evaluate({32, 0, 0, 0}, 0, t_half) == 4);

// late coef
static_assert(evaluate({0, 0, 0, 1024}, 0, t_half) == 1024);

// all coeffs
static_assert(evaluate({1024, 2048, 4096, 8192}, 0, t_half) == 10880);

// negative a
static_assert(evaluate({-16, 0, 0, 4}, 0, t_half) == 2);

// negative b
static_assert(evaluate({0, -10, 10, 0}, 0, t_half) == 3);

// negative c
static_assert(evaluate({0, 100, -12, 0}, 0, t_half) == 19);

// negative d
static_assert(evaluate({0, 0, 62, -20}, 0, t_half) == 11);

// t = 0 -> C3
static_assert(evaluate({9999, -8888, 7777, 35}, 0, t0) == 35);

// t = 1/4
static_assert(evaluate({1024, 1024, 1024, 1024}, 0, t_quarter) == 1360);

// t = 0xAAAA... (~2/3), isolates truncation behavior on non-power-of-two fractions
static_assert(evaluate({0, 0, 81, 0}, 0, x_t::literal(0xAAAAAAAAAAAAULL)) == 54);

// t = 3/4
static_assert(evaluate({256, 256, 256, 256}, 0, t_three_quarter) == 700);

// t = max -> C0(1 - epsilon) + C1(1 - epsilon) + C2(1 - epsilon) + C3 = C0 + C1 + C2 + C3
static_assert(evaluate({1024, 2048, 4096, 8192}, 0, t_max) == 15360);

// coeff[0] survives bit packing shift round-trip
constexpr auto large_cubic_coeff = coeff_t{50};
static_assert(sut_t{{large_cubic_coeff, c0, c0, c0}, 5}.evaluate(x_t{16}).value == (large_cubic_coeff.value >> 3));

// c0: just check sign; anything more would be a tautology
static_assert(evaluate({c0_max, 0, 0, 0}, 0, t_max) > 0);
static_assert(evaluate({c0_min, 0, 0, 0}, 0, t_max) < 0);

// c3
static_assert(evaluate({0, 0, 0, c_max}, 0, t0) == c_max);
static_assert(evaluate({0, 0, 0, c_min}, 0, t0) == c_min);

// dense polynomial does not overflow
constexpr auto c0_quarter = c0_max / 4;
constexpr auto c_quarter = c_max / 4;
static_assert(evaluate({c0_quarter, c_quarter, c_quarter, c_quarter}, 0, t_max) > (c_max / 2));

// saturation ceiling
static_assert(evaluate({c0_max, c_max, c_max, c_max}, 0, t_max) == c_max);
static_assert(evaluate({c0_min, c_min, c_min, c_min}, 0, t_max) == c_min);

constexpr auto rounding_coeffs = coeffs_t{c0, c0, c1, c0};

// 16/4 -> 4.00, no rounding
static_assert(evaluate(rounding_coeffs, 2, x_t::literal(16)) == 4);

// 17/4 -> 4.25, rounds down
static_assert(evaluate(rounding_coeffs, 2, x_t::literal(17)) == 4);

// 18/4 -> 4.50, half up rounds up; truncate would round down
static_assert(evaluate(rounding_coeffs, 2, x_t::literal(18)) == 5);

// 19/4 -> 4.75, half up rounds up
static_assert(evaluate(rounding_coeffs, 2, x_t::literal(19)) == 5);

} // namespace evaluation_tests

// --------------------------------------------------------------------------------------------------------------------
// extend_final_tangent
// --------------------------------------------------------------------------------------------------------------------

namespace extend_final_tangent_tests {

static_assert(sut_t{{coeff_t{0}, coeff_t{0}, coeff_t{3}, coeff_t{5}}, 2}.extend_final_tangent(x_t{7})
    == sut_t::y_t::convert(coeff_t{8} + coeff_t{3} * (x_t{7} >> 2)));

static_assert(sut_t{{coeff_t{5}, coeff_t{7}, coeff_t{11}, coeff_t{13}}, -2}.extend_final_tangent(x_t{17})
    == sut_t::y_t::convert(coeff_t{36} + coeff_t{40} * (x_t{17} << 2)));

} // namespace extend_final_tangent_tests

// --------------------------------------------------------------------------------------------------------------------
// width
// --------------------------------------------------------------------------------------------------------------------

namespace width_tests {

// bounds of valid log2_widths: [-frac_bits, bits-frac_bits-2] = [-48, 14] for fixed_t<int64_t, 48>
static_assert(sut_t{coeffs, -48}.width() == x_t{1} >> 48);
static_assert(sut_t{coeffs, -47}.width() == x_t{1} >> 47);
static_assert(sut_t{coeffs, -1}.width() == x_t{1} >> 1);
static_assert(sut_t{coeffs, 0}.width() == x_t{1} << 0);
static_assert(sut_t{coeffs, 1}.width() == x_t{1} << 1);
static_assert(sut_t{coeffs, 13}.width() == x_t{1} << 13);
static_assert(sut_t{coeffs, 14}.width() == x_t{1} << 14);

} // namespace width_tests

// --------------------------------------------------------------------------------------------------------------------
// is_monotonic
// --------------------------------------------------------------------------------------------------------------------

namespace is_monotonic_tests {

// returns true if a segment with these coefficients is monotonic
//
// d translates the curve up/down but does not affect slope or monotonicity.
// log2_width does not affect the mathematical shape mapped to [0, 1].
constexpr auto is_monotonic(int64_t a, int64_t b, int64_t c) -> bool
{
    return sut_t{{coeff_t{a}, coeff_t{b}, coeff_t{c}, coeff_t{0}}, 0}.is_monotonic();
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
constexpr auto large_val = coeff_value_t{1} << 20;
static_assert(is_monotonic(large_val, -large_val, large_val));

} // namespace is_monotonic_tests

// --------------------------------------------------------------------------------------------------------------------
// is_valid
// --------------------------------------------------------------------------------------------------------------------

namespace is_valid_tests {

// zero is a valid shift
static_assert(sut_t{coeffs, 0}.is_valid());

// bounds of valid log2_widths: [-frac_bits, bits-frac_bits-2] = [-48, 14] for fixed_t<int64_t, 48>
static_assert(!sut_t{coeffs, -49}.is_valid());
static_assert(sut_t{coeffs, -48}.is_valid());
static_assert(sut_t{coeffs, -47}.is_valid());
static_assert(sut_t{coeffs, -1}.is_valid());
static_assert(sut_t{coeffs, 0}.is_valid());
static_assert(sut_t{coeffs, 1}.is_valid());
static_assert(sut_t{coeffs, 13}.is_valid());
static_assert(sut_t{coeffs, 14}.is_valid());
static_assert(!sut_t{coeffs, 15}.is_valid());

// strictly out of bounds (>= 64 or <= -64)
static_assert(!sut_t{coeffs, 64}.is_valid());
static_assert(!sut_t{coeffs, -64}.is_valid());

// capacity for int8_t
static_assert(!sut_t{coeffs, 127}.is_valid());
static_assert(!sut_t{coeffs, -128}.is_valid());

} // namespace is_valid_tests

// --------------------------------------------------------------------------------------------------------------------
// death tests
// --------------------------------------------------------------------------------------------------------------------

#if defined CRV_ENABLE_DEATH_TESTS

namespace death_tests {

TEST(spline_segment, violates_dx_lower_bound)
{
    // dx = -1.0; it is unconditionally out of bounds
    constexpr auto sut = sut_t({coeff_t{0}, coeff_t{0}, coeff_t{0}, coeff_t{0}}, 0);
    EXPECT_DEBUG_DEATH(static_cast<void>(sut.evaluate(x_t::literal(-1))), "<= dx");
}

TEST(spline_segment, violates_t_upper_bound)
{
    // dx is 1.0, but dividing by 2^-1 makes t = 2.0, which is out of bounds
    constexpr auto sut = sut_t({coeff_t{0}, coeff_t{0}, coeff_t{0}, coeff_t{0}}, -1);
    constexpr auto dx_one = x_t::literal(1LL << x_t::frac_bits);
    EXPECT_DEBUG_DEATH(static_cast<void>(sut.evaluate(dx_one)), "dx <");
}

TEST(spline_segment, violates_coeff0_positive_packing_bounds)
{
    // this is safe and does not assert
    [[maybe_unused]] auto safe = sut_t({coeff_t{127}, coeff_t{0}, coeff_t{0}, coeff_t{0}}, 0);

    // coeff0 is too large to safely shift left by 8 to store coeff0
    EXPECT_DEBUG_DEATH(sut_t({coeff_t{128}, coeff_t{0}, coeff_t{0}, coeff_t{0}}, 0), "top 8 bits");
}

TEST(spline_segment, violates_coeff0_negative_packing_bounds)
{
    // this is safe and does not assert
    [[maybe_unused]] auto safe_neg = sut_t({coeff_t{-128}, coeff_t{0}, coeff_t{0}, coeff_t{0}}, 0);

    // coeff0 is too large to safely shift left by 8 to store coeff0
    EXPECT_DEBUG_DEATH(sut_t({coeff_t{-129}, coeff_t{0}, coeff_t{0}, coeff_t{0}}, 0), "top 8 bits");
}

} // namespace death_tests

#endif

} // namespace
} // namespace crv::spline
