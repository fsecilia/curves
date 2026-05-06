// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment.hpp"
#include <crv/math/fixed/io.hpp>
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>
#include <tuple>

namespace crv::spline {
namespace {

// ====================================================================================================================
// packed_segment_t
// ====================================================================================================================

namespace packed_segment_test {

using coeff_value_t = int64_t;
using coeff_t = fixed_t<coeff_value_t, 45>;
using sut_t = packed_segment_t<coeff_t>;
using coeffs_t = sut_t::coeffs_t;

constexpr auto c1 = coeff_t{1};
constexpr auto cv_max = max<coeff_value_t>();
constexpr auto cv_min = min<coeff_value_t>();
constexpr auto cv0_max = coeff_value_t{cv_max >> 8};
constexpr auto cv0_min = coeff_value_t{cv_min >> 8};

// --------------------------------------------------------------------------------------------------------------------
// packed values
// --------------------------------------------------------------------------------------------------------------------

constexpr auto unpack_coeffs(sut_t const& sut) noexcept -> coeffs_t
{
    return {sut.unpack_coeff0(), sut.unpack_coeff1(), sut.unpack_coeff2(), sut.unpack_coeff3()};
}

constexpr auto test_unpacked(coeffs_t coeffs, int8_t log2_width) noexcept -> bool
{
    auto const sut = sut_t{coeffs, log2_width};
    return unpack_coeffs(sut) == coeffs && sut.unpack_log2_width() == log2_width;
}

constexpr auto log2_width_max = max<int8_t>();
constexpr auto log2_width_min = min<int8_t>();

// zero
static_assert(test_unpacked(coeffs_t{}, 0));

// sign extension
constexpr auto sign_extension_coeffs = coeffs_t{-c1, -c1, -c1, -c1};
static_assert(test_unpacked(sign_extension_coeffs, log2_width_min));
static_assert(test_unpacked(sign_extension_coeffs, log2_width_max));

// safe boundaries
constexpr auto boundaries_min = coeffs_t{coeff_t{cv0_min}, coeff_t{cv_min}, coeff_t{cv_min}, coeff_t{cv_min}};
constexpr auto boundaries_max = coeffs_t{coeff_t{cv0_max}, coeff_t{cv_max}, coeff_t{cv_max}, coeff_t{cv_max}};
static_assert(test_unpacked(boundaries_min, log2_width_min));
static_assert(test_unpacked(boundaries_min, 0));
static_assert(test_unpacked(boundaries_min, log2_width_max));
static_assert(test_unpacked(boundaries_max, log2_width_min));
static_assert(test_unpacked(boundaries_max, 0));
static_assert(test_unpacked(boundaries_max, log2_width_max));

// combs: test bitwise ops aren't bleeding into adjacent bits
constexpr auto comb_even = coeff_t{0x2AAA'AAAA'AAAA'AAAALL};
constexpr auto comb_odd = coeff_t{0x5555'5555'5555'5555LL};

static_assert(test_unpacked({comb_even >> 8, comb_odd, comb_even, comb_odd}, 0x55));
static_assert(test_unpacked({comb_odd >> 8, comb_even, comb_odd, comb_even}, 0xAA));

#if defined CRV_ENABLE_DEATH_TESTS

namespace death_tests {

// coeff[0] packs log2_width into bottom 8 bits, so underlying must fit in the remaining, with sign
constexpr auto coeff_bits = sizeof(typename coeff_t::value_t) * CHAR_BIT;
constexpr auto coeff0_unsafe_bound = coeff_t::value_t{1} << (coeff_bits - 8 - 1);

TEST(spline_packed_segment, violates_coeff0_positive_packing_bounds)
{
    // this is safe and does not assert
    [[maybe_unused]] auto safe
        = sut_t({coeff_t::literal(coeff0_unsafe_bound - 1), coeff_t{0}, coeff_t{0}, coeff_t{0}}, 0);

    // coeff0 is too large to safely shift left by 8 to store coeff0
    EXPECT_DEBUG_DEATH(
        sut_t({coeff_t::literal(coeff0_unsafe_bound), coeff_t{0}, coeff_t{0}, coeff_t{0}}, 0), "top 8 bits");
}

TEST(spline_packed_segment, violates_coeff0_negative_packing_bounds)
{
    // this is safe and does not assert
    [[maybe_unused]] auto safe_neg
        = sut_t({coeff_t::literal(-coeff0_unsafe_bound), coeff_t{0}, coeff_t{0}, coeff_t{0}}, 0);

    // coeff0 is too large to safely shift left by 8 to store coeff0
    EXPECT_DEBUG_DEATH(
        sut_t({coeff_t::literal(-coeff0_unsafe_bound - 1), coeff_t{0}, coeff_t{0}, coeff_t{0}}, 0), "top 8 bits");
}

} // namespace death_tests

#endif

} // namespace packed_segment_test

// ====================================================================================================================
// segment_t
// ====================================================================================================================

namespace segment_test {

using coeff_value_t = int64_t;
using coeff_t = fixed_t<coeff_value_t, 45>;

using x_value_t = int64_t;
using x_t = fixed_t<x_value_t, 48>;
using y_t = x_t;

using normalized_t = fixed_t<uint64_t, 64>;
using polynomial_evaluator_t = polynomial_evaluator_t<mac_t{}>;

struct packed_segment_t
{
    using coeff_t = segment_test::coeff_t;
    using coeffs_t = cubic_polynomial_t<coeff_t>;

    coeffs_t coeffs;
    int_t log2_width;

    constexpr auto unpack_log2_width() const noexcept -> int_t { return log2_width; }
    constexpr auto unpack_coeff0() const noexcept -> coeff_t { return coeffs[0]; }
    constexpr auto unpack_coeff1() const noexcept -> coeff_t { return coeffs[1]; }
    constexpr auto unpack_coeff2() const noexcept -> coeff_t { return coeffs[2]; }
    constexpr auto unpack_coeff3() const noexcept -> coeff_t { return coeffs[3]; }
};

using sut_t = segment_t<x_t, y_t, coeff_t, normalized_t, packed_segment_t, polynomial_evaluator_t{}>;
using coeffs_t = sut_t::coeffs_t;
using vals_t = std::array<coeff_t::value_t, std::tuple_size_v<coeffs_t>>;

constexpr auto sut(coeffs_t coeffs, int_t log2_width) noexcept -> sut_t
{
    return sut_t{packed_segment_t{coeffs, log2_width}};
}

constexpr auto sut(vals_t vals, int_t log2_width) noexcept -> sut_t
{
    return sut(
        coeffs_t{
            coeff_t::literal(vals[0]),
            coeff_t::literal(vals[1]),
            coeff_t::literal(vals[2]),
            coeff_t::literal(vals[3]),
        },
        log2_width);
}

constexpr auto c0 = coeff_t{0};
constexpr auto c1 = coeff_t{1};
constexpr auto cv_max = max<coeff_value_t>();
constexpr auto cv_min = min<coeff_value_t>();

// 56-bit bounds for C0 since it also packs log2_width
constexpr auto cv0_max = coeff_value_t{cv_max >> 8};
constexpr auto cv0_min = coeff_value_t{cv_min >> 8};

constexpr auto t0 = normalized_t::literal(0);
constexpr auto t_half = normalized_t::literal(1LL << (normalized_t::frac_bits - 1));
constexpr auto t_quarter = normalized_t::literal(1LL << (normalized_t::frac_bits - 2));
constexpr auto t_three_quarter = t_half + t_quarter;
constexpr auto t_max = max<normalized_t>();

constexpr auto coeffs = coeffs_t{};

// --------------------------------------------------------------------------------------------------------------------
// x_to_t
// --------------------------------------------------------------------------------------------------------------------

namespace x_to_t_tests {

// zero shift
constexpr auto x_quarter = x_t::literal(1LL << (x_t::frac_bits - 2));
constexpr auto sut0 = sut({c0, c0, c1, c0}, 0);
static_assert(sut0.x_to_t(x_quarter) == normalized_t::convert(x_quarter));

// positive shift (left shift x, segment is narrower than 1.0)
// x is 1/8, shift is 2 (multiply by 4), resulting t should be 0.5
constexpr auto x_eighth = x_t::literal(1LL << (x_t::frac_bits - 3));
constexpr auto sut_left = sut({c0, c0, c1, c0}, -2);
static_assert(sut_left.x_to_t(x_eighth) == normalized_t::convert(x_t::literal(x_eighth.value << 2)));

// negative shift (right shift x, segment is wider than 1.0)
// x is 2.0, shift is -2 (divide by 4), resulting t should be 0.5
constexpr auto x_two = x_t::literal(2ULL << x_t::frac_bits);
constexpr auto sut_right = sut({c0, c0, c1, c0}, 2);
static_assert(sut_right.x_to_t(x_two) == normalized_t::convert(x_t::literal(x_two.value >> 2)));

// exactly zero should yield exactly zero regardless of shift
constexpr auto x_zero = x_t::literal(0);
static_assert(sut0.x_to_t(x_zero) == t0);
static_assert(sut_left.x_to_t(x_zero) == t0);

// upper bound approach
// x just below width() should yield something very close to t_max
constexpr auto x_almost_width = x_t::literal(sut0.width().value - 1);
static_assert(max<normalized_t>().value - sut0.x_to_t(x_almost_width).value
    == (normalized_t::convert(x_t::literal(1)).value - 1));
static_assert(sut0.x_to_t(x_almost_width)
    == max<normalized_t>() - normalized_t::convert(x_t::literal(1)) + normalized_t::literal(1));

} // namespace x_to_t_tests

// --------------------------------------------------------------------------------------------------------------------
// primal evaluation
// --------------------------------------------------------------------------------------------------------------------

namespace primal_tests {

constexpr auto evaluate(sut_t const& sut, normalized_t t) noexcept -> y_t
{
    return sut.primal(t);
}

constexpr auto evaluate(coeffs_t const& coeffs, int8_t log2_width, normalized_t t) noexcept -> y_t
{
    return evaluate(sut(coeffs, log2_width), t);
}

constexpr auto evaluate(vals_t const& vals, int8_t log2_width, normalized_t t) noexcept -> y_t
{
    return evaluate(sut(vals, log2_width), t);
}

// expected polynomial output at coeff_t scale, lifted to y_t. Decouples expectations from y_t::frac_bits.
constexpr auto expected(coeff_t::value_t coeff_value) -> y_t
{
    return y_t::convert(coeff_t::literal(coeff_value));
}

// all zeros
static_assert(evaluate({0, 0, 0, 0}, 0, t_half) == expected(0));

// early coeff
static_assert(evaluate({32, 0, 0, 0}, 0, t_half) == expected(4));

// late coef
static_assert(evaluate({0, 0, 0, 1024}, 0, t_half) == expected(1024));

// all coeffs
static_assert(evaluate({1024, 2048, 4096, 8192}, 0, t_half) == expected(10880));

// negative a
static_assert(evaluate({-16, 0, 0, 4}, 0, t_half) == expected(2));

// negative b
static_assert(evaluate({0, -10, 10, 0}, 0, t_half) == expected(3));

// negative c
static_assert(evaluate({0, 100, -12, 0}, 0, t_half) == expected(19));

// negative d
static_assert(evaluate({0, 0, 62, -20}, 0, t_half) == expected(11));

// t = 0 -> C3
static_assert(evaluate({9999, -8888, 7777, 35}, 0, t0) == expected(35));

// t = 1/4
static_assert(evaluate({1024, 1024, 1024, 1024}, 0, t_quarter) == expected(1360));

// t = 0xAAAA... (~2/3), isolates truncation behavior on non-power-of-two fractions
static_assert(evaluate({0, 0, 81, 0}, 0, normalized_t::literal(0xAAAAAAAAAAAAAAAAULL)).value == expected(54).value);

// t = 3/4
static_assert(evaluate({256, 256, 256, 256}, 0, t_three_quarter) == expected(700));

// t = max -> C0 + C1 + C2 + C3 because of round half up
static_assert(evaluate({1024, 2048, 4096, 8192}, 0, t_max) == expected(15360));

// coeff[0] survives bit packing shift round-trip
constexpr auto c_large_cubic = coeff_t{50};
static_assert(evaluate({c_large_cubic, c0, c0, c0}, 5, t_half) == expected(c_large_cubic.value >> 3));

// c0: just check sign; anything more would be a tautology
static_assert(evaluate({cv0_max, 0, 0, 0}, 0, t_max).value > 0);
static_assert(evaluate({cv0_min, 0, 0, 0}, 0, t_max).value < 0);

// c3
static_assert(evaluate({0, 0, 0, cv_max}, 0, t0) == expected(cv_max));
static_assert(evaluate({0, 0, 0, cv_min}, 0, t0) == expected(cv_min));

// dense polynomial does not overflow
constexpr auto cv0_quarter = cv0_max / 4;
constexpr auto cv_quarter = cv_max / 4;
static_assert(evaluate({cv0_quarter, cv_quarter, cv_quarter, cv_quarter}, 0, t_max).value > (cv_max / 2));

// rounding boundary tests
//
// The final Horner mac computes round_half_up(c2.value*t.value / 2^normalized_t::frac_bits) in coeff_t. With
// c2 = coeff_t{1}, a unit step in the output covers t_per_v t.value units, and round-half-up centers each output range
// on v*t_per_v. Picking v_center away from the boundaries, the contiguous t.value range that rounds to v_center is
// [v_center*t_per_v - t_per_v/2, v_center*t_per_v + t_per_v/2). Here, we probe one epsilon-t past each side.
constexpr auto rounding_coeffs = coeffs_t{c0, c0, c1, c0};
constexpr auto t_per_v = normalized_t::value_t{1} << (normalized_t::frac_bits - coeff_t::frac_bits);
constexpr auto half_t_per_v = t_per_v / 2;
constexpr auto v_center = coeff_t::value_t{16};
constexpr auto v_center_t_start = v_center * t_per_v - half_t_per_v;
constexpr auto v_center_t_end = v_center * t_per_v + half_t_per_v;

// last t in previous range still rounds down to v_center - 1
static_assert(evaluate(rounding_coeffs, 0, normalized_t::literal(v_center_t_start - 1)) == expected(v_center - 1));

// first t in current range rounds up to v_center
static_assert(evaluate(rounding_coeffs, 0, normalized_t::literal(v_center_t_start)) == expected(v_center));

// last t in current range still rounds to v_center
static_assert(evaluate(rounding_coeffs, 0, normalized_t::literal(v_center_t_end - 1)) == expected(v_center));

// first t in next range rounds up to v_center + 1
static_assert(evaluate(rounding_coeffs, 0, normalized_t::literal(v_center_t_end)) == expected(v_center + 1));

} // namespace primal_tests

// --------------------------------------------------------------------------------------------------------------------
// tangent evaluation
// --------------------------------------------------------------------------------------------------------------------

namespace tangent_tests {

constexpr auto evaluate_tangent(vals_t const& vals, int8_t log2_width, normalized_t t) noexcept -> y_t
{
    return sut(vals, log2_width).tangent(t);
}

constexpr auto expected(coeff_t::value_t coeff_value) -> y_t
{
    return y_t::convert(coeff_t::literal(coeff_value));
}

// baseline
static_assert(evaluate_tangent({0, 0, 0, 0}, 0, t_half) == expected(0));

// constant tangent (only C2 is non-zero)
// derivative of C2 * t is C2
static_assert(evaluate_tangent({0, 0, 37, 0}, 0, t0) == expected(37));
static_assert(evaluate_tangent({0, 0, 37, 0}, 0, t_half) == expected(37));
static_assert(evaluate_tangent({0, 0, 37, 0}, 0, t_max) == expected(37));

// linear tangent (only C1 is non-zero)
// derivative of C1 * t^2 is 2 * C1 * t
static_assert(evaluate_tangent({0, 100, 0, 0}, 0, t0) == expected(0));
static_assert(evaluate_tangent({0, 100, 0, 0}, 0, t_half) == expected(100));
static_assert(evaluate_tangent({0, 100, 0, 0}, 0, t_max) == expected(200));

// quadratic tangent (only C0 is non-zero)
// derivative of C0 * t^3 is 3 * C0 * t^2
static_assert(evaluate_tangent({100, 0, 0, 0}, 0, t0) == expected(0));
static_assert(evaluate_tangent({100, 0, 0, 0}, 0, t_half) == expected(75));
static_assert(evaluate_tangent({100, 0, 0, 0}, 0, t_max) == expected(300));

// full polynomial integration
// y'(t) = 3*100*t^2 + 2*50*t + 25
// at t=0.5 -> 3*100*(0.25) + 2*50*(0.5) + 25 = 75 + 50 + 25 = 150
static_assert(evaluate_tangent({100, 50, 25, 0}, 0, t_half) == expected(150));

// polarity
static_assert(evaluate_tangent({-100, -50, -25, 0}, 0, t_half) == expected(-150));

// final coefficient does not affect tangent
static_assert(evaluate_tangent({0, 0, 0, 10000}, 0, t_half) == expected(0));

} // namespace tangent_tests

// --------------------------------------------------------------------------------------------------------------------
// extend_final_tangent
// --------------------------------------------------------------------------------------------------------------------

namespace extend_final_tangent_tests {

static_assert(sut({coeff_t{0}, coeff_t{0}, coeff_t{3}, coeff_t{5}}, 2).extend_final_tangent(x_t{7})
    == sut_t::y_t::convert(coeff_t{8} + coeff_t{3} * (x_t{7} >> 2)));

static_assert(sut({coeff_t{5}, coeff_t{7}, coeff_t{11}, coeff_t{13}}, -2).extend_final_tangent(x_t{17})
    == sut_t::y_t::convert(coeff_t{36} + coeff_t{40} * (x_t{17} << 2)));

// at x=0 is the same as t=1: 5 + 7 + 11 + 13
static_assert(sut({coeff_t{5}, coeff_t{7}, coeff_t{11}, coeff_t{13}}, 0).extend_final_tangent(x_t{0})
    == sut_t::y_t::convert(coeff_t{36}));

} // namespace extend_final_tangent_tests

// --------------------------------------------------------------------------------------------------------------------
// width
// --------------------------------------------------------------------------------------------------------------------

namespace width_tests {

// bounds of valid widths for fixed_t<int64_t, 48>: 2^[-frac_bits, bits-frac_bits-2] = 2^[-48, 14]
static_assert(sut(coeffs, -48).width() == x_t{1} >> 48);
static_assert(sut(coeffs, -47).width() == x_t{1} >> 47);
static_assert(sut(coeffs, -1).width() == x_t{1} >> 1);
static_assert(sut(coeffs, 0).width() == x_t{1} << 0);
static_assert(sut(coeffs, 1).width() == x_t{1} << 1);
static_assert(sut(coeffs, 13).width() == x_t{1} << 13);
static_assert(sut(coeffs, 14).width() == x_t{1} << 14);

} // namespace width_tests

// --------------------------------------------------------------------------------------------------------------------
// is_valid
// --------------------------------------------------------------------------------------------------------------------

namespace is_valid_tests {

// zero is a valid shift
static_assert(sut(coeffs, 0).is_valid());

// bounds of valid log2_widths: [-frac_bits, bits-frac_bits-2] = [-48, 14] for fixed_t<int64_t, 48>
static_assert(!sut(coeffs, -49).is_valid());
static_assert(sut(coeffs, -48).is_valid());
static_assert(sut(coeffs, -47).is_valid());
static_assert(sut(coeffs, -1).is_valid());
static_assert(sut(coeffs, 0).is_valid());
static_assert(sut(coeffs, 1).is_valid());
static_assert(sut(coeffs, 13).is_valid());
static_assert(sut(coeffs, 14).is_valid());
static_assert(!sut(coeffs, 15).is_valid());

// strictly out of bounds (>= 64 or <= -64)
static_assert(!sut(coeffs, 64).is_valid());
static_assert(!sut(coeffs, -64).is_valid());

// capacity for int8_t
static_assert(!sut(coeffs, 127).is_valid());
static_assert(!sut(coeffs, -128).is_valid());

} // namespace is_valid_tests

// --------------------------------------------------------------------------------------------------------------------
// death tests
// --------------------------------------------------------------------------------------------------------------------

#if defined CRV_ENABLE_DEATH_TESTS

namespace death_tests {

TEST(spline_segment, violates_x_lower_bound)
{
    // x = -1.0; it is unconditionally out of bounds
    constexpr auto sut = segment_test::sut({0, 0, 0, 0}, 0);
    EXPECT_DEBUG_DEATH(static_cast<void>(sut.x_to_t(x_t::literal(-1))), "<= x");
}

TEST(spline_segment, violates_t_upper_bound)
{
    // x is 1.0, but dividing by 2^-1 makes t = 2.0, which is out of bounds
    constexpr auto sut = segment_test::sut({0, 0, 0, 0}, -1);
    constexpr auto x_one = x_t{1};
    EXPECT_DEBUG_DEATH(static_cast<void>(sut.x_to_t(x_one)), "x <");
}

TEST(spline_segment, violates_extend_final_tangent_lower_bound)
{
    constexpr auto sut = segment_test::sut({0, 0, 0, 0}, 0);
    EXPECT_DEBUG_DEATH(static_cast<void>(sut.extend_final_tangent(x_t::literal(-1))), "<= x");
}

} // namespace death_tests

#endif

} // namespace segment_test

} // namespace
} // namespace crv::spline
