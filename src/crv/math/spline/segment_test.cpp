// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment.hpp"
#include <crv/math/fixed/io.hpp>
#include <crv/math/limits.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// segment_input_normalizer_t
// --------------------------------------------------------------------------------------------------------------------

namespace segment_input_normalizer {

using normalized_t = fixed_t<uint16_t, 16>;
using x_t = fixed_t<int32_t, 8>;

// combines inputs predictably
struct shifter_t
{
    template <typename value_t> constexpr auto shift(value_t value, int_t count) const noexcept -> value_t
    {
        return int_cast<value_t>((value * 100) + count);
    }
};

constexpr auto test_segment_input_normalizer() -> bool
{
    auto const normalizer = segment_input_normalizer_t<normalized_t, shifter_t{}>{};

    auto const x = x_t::literal(10);
    auto const log2_width = int_t{3};

    // x_to_t_shift = normalized_t::frac_bits - x_t::frac_bits - log2_width = 16 - 8 - 3 = 5
    // shifter input: value = 10, count = 5
    // shifter output: (10*100) + 5 = 1005
    auto const expected = int_t{1005};

    auto const actual = normalizer(x, log2_width);

    return actual.value == expected;
}
static_assert(test_segment_input_normalizer());

} // namespace segment_input_normalizer

// --------------------------------------------------------------------------------------------------------------------
// segment_t
// --------------------------------------------------------------------------------------------------------------------

namespace segment_test {

using coeff_value_t = int64_t;
using coeff_t = fixed_t<coeff_value_t, 45>;

using x_value_t = int64_t;
using x_t = fixed_t<x_value_t, 48>;
using y_t = x_t;

using sut_t = segment_t<x_t, y_t, coeff_t>;
using coeffs_t = sut_t::coeffs_t;
using vals_t = std::array<coeff_t::value_t, sut_t::coeff_count>;

constexpr auto c0 = coeff_t{0};
constexpr auto c1 = coeff_t{1};
constexpr auto cv_max = max<coeff_value_t>();
constexpr auto cv_min = min<coeff_value_t>();

// 56-bit bounds for C0 since it also packs log2_width
constexpr auto c01 = coeff_t{1};
constexpr auto cv0_max = coeff_value_t{cv_max >> 8};
constexpr auto cv0_min = coeff_value_t{cv_min >> 8};

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

using sut_t = segment_t<x_t, coeff_t, coeff_t>;

// zero shift
constexpr auto dx_quarter = x_t::literal(1LL << (x_t::frac_bits - 2));
constexpr auto sut0 = sut_t{{c0, c0, c1, c0}, 0};
static_assert(sut0.evaluate(dx_quarter) == coeff_t::convert(dx_quarter));

// positive shift (left shift dx, segment is narrower than 1.0)
// dx is 1/8, shift is 2 (multiply by 4), resulting t should be 0.5
constexpr auto dx_eighth = x_t::literal(1LL << (x_t::frac_bits - 3));
constexpr auto sut_left = sut_t{{c0, c0, c1, c0}, -2};
static_assert(sut_left.evaluate(dx_eighth) == coeff_t::convert(x_t::literal(dx_eighth.value << 2)));

// negative shift (right shift dx, segment is wider than 1.0)
// dx is 2.0, shift is -2 (divide by 4), resulting t should be 0.5
constexpr auto dx_two = x_t::literal(2ULL << x_t::frac_bits);
constexpr auto sut_right = sut_t{{c0, c0, c1, c0}, 2};
static_assert(sut_right.evaluate(dx_two) == coeff_t::convert(x_t::literal(dx_two.value >> 2)));

} // namespace normalization_shift_tests

// --------------------------------------------------------------------------------------------------------------------
// polynomial evaluation
// --------------------------------------------------------------------------------------------------------------------

constexpr auto evaluate(coeffs_t const& coeffs, int8_t log2_width, x_t dx) noexcept -> y_t
{
    return sut_t{coeffs, log2_width}.evaluate(dx);
}

constexpr auto evaluate(vals_t const& coeff_vals, int8_t log2_width, x_t dx) noexcept -> y_t
{
    return evaluate(coeffs_t{coeff_t::literal(coeff_vals[0]), coeff_t::literal(coeff_vals[1]),
                        coeff_t::literal(coeff_vals[2]), coeff_t::literal(coeff_vals[3])},
        log2_width, dx);
}

namespace evaluation_tests {

// expected polynomial output at coeff_t scale, lifted to y_t. Decouples expectations from y_t::frac_bits.
constexpr auto expected = [](coeff_t::value_t coeff_value) { return y_t::convert(coeff_t::literal(coeff_value)); };

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
static_assert(evaluate({0, -10, 10, 0}, 0, t_half) == expected(2));

// negative c
static_assert(evaluate({0, 100, -12, 0}, 0, t_half) == expected(19));

// negative d
static_assert(evaluate({0, 0, 62, -20}, 0, t_half) == expected(11));

// t = 0 -> C3
static_assert(evaluate({9999, -8888, 7777, 35}, 0, t0) == expected(35));

// t = 1/4
static_assert(evaluate({1024, 1024, 1024, 1024}, 0, t_quarter) == expected(1360));

// t = 0xAAAA... (~2/3), isolates truncation behavior on non-power-of-two fractions
static_assert(evaluate({0, 0, 81, 0}, 0, x_t::literal(0xAAAAAAAAAAAAULL)) == expected(53));

// t = 3/4
static_assert(evaluate({256, 256, 256, 256}, 0, t_three_quarter) == expected(700));

// t = max -> C0(1 - epsilon) + C1(1 - epsilon) + C2(1 - epsilon) + C3 = C0 + C1 + C2 + C3 - 3*epsilon
static_assert(evaluate({1024, 2048, 4096, 8192}, 0, t_max) == expected(15357));

// coeff[0] survives bit packing shift round-trip
constexpr auto c_large_cubic = coeff_t{50};
static_assert(sut_t{{c_large_cubic, c0, c0, c0}, 5}.evaluate(x_t{16}) == expected(c_large_cubic.value >> 3));

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

// The rounding tests below use small, raw dx values (16..19) so that t.value lands at the dx_to_t rounding boundary.
// When coeff_t is narrower than x_t, those t values sit below coeff_t's representability floor
// (2^-coeff_t::frac_bits), so storing c2*t back into coeff_t between Horner rounds collapses the result to zero and
// the rounding behavior becomes invisible. Scaling c2 by 2^(x_t::frac_bits - coeff_t::frac_bits) lifts the intermediate
// result into coeff_t's representable range. With same frac_bits, this reduces to coeff_t{1}.
constexpr auto c2_unit = coeff_t::literal(coeff_t::value_t{1} << x_t::frac_bits);
constexpr auto rounding_coeffs = coeffs_t{c0, c0, c2_unit, c0};

// 16/4 -> 4.00, no rounding
static_assert(evaluate(rounding_coeffs, 2, x_t::literal(16)) == expected(4));

// 17/4 -> 4.25, rounds down
static_assert(evaluate(rounding_coeffs, 2, x_t::literal(17)) == expected(4));

// 18/4 -> 4.50, half up rounds up; truncate would round down
static_assert(evaluate(rounding_coeffs, 2, x_t::literal(18)) == expected(5));

// 19/4 -> 4.75, half up rounds up
static_assert(evaluate(rounding_coeffs, 2, x_t::literal(19)) == expected(5));

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
// packed values
// --------------------------------------------------------------------------------------------------------------------

namespace packed_values_tests {

constexpr auto test_coeffs(coeffs_t coeffs, int8_t log2_width) noexcept -> bool
{
    auto const sut = sut_t{coeffs, log2_width};
    return sut.coeffs() == coeffs && sut.log2_width() == log2_width;
}

constexpr auto log2_width_max = max<int8_t>();
constexpr auto log2_width_min = min<int8_t>();

// zero
static_assert(test_coeffs(coeffs, 0));

// sign extension
constexpr auto sign_extension_coeffs = coeffs_t{-c01, -c1, -c1, -c1};
static_assert(test_coeffs(sign_extension_coeffs, log2_width_min));
static_assert(test_coeffs(sign_extension_coeffs, log2_width_max));

// safe boundaries
constexpr auto boundaries_min = coeffs_t{coeff_t{cv0_min}, coeff_t{cv_min}, coeff_t{cv_min}, coeff_t{cv_min}};
constexpr auto boundaries_max = coeffs_t{coeff_t{cv0_max}, coeff_t{cv_max}, coeff_t{cv_max}, coeff_t{cv_max}};
static_assert(test_coeffs(boundaries_min, log2_width_min));
static_assert(test_coeffs(boundaries_min, 0));
static_assert(test_coeffs(boundaries_min, log2_width_max));
static_assert(test_coeffs(boundaries_max, log2_width_min));
static_assert(test_coeffs(boundaries_max, 0));
static_assert(test_coeffs(boundaries_max, log2_width_max));

// combs: test bitwise ops aren't bleeding into adjacent bits
constexpr auto comb_even = coeff_t{0x2AAA'AAAA'AAAA'AAAALL};
constexpr auto comb_odd = coeff_t{0x5555'5555'5555'5555LL};

static_assert(test_coeffs({comb_even >> 8, comb_odd, comb_even, comb_odd}, 0xAA));
static_assert(test_coeffs({comb_odd >> 8, comb_even, comb_odd, comb_even}, 0x55));

} // namespace packed_values_tests

// --------------------------------------------------------------------------------------------------------------------
// width
// --------------------------------------------------------------------------------------------------------------------

namespace width_tests {

// bounds of valid widths for fixed_t<int64_t, 48>: 2^[-frac_bits, bits-frac_bits-2] = 2^[-48, 14]
static_assert(sut_t{coeffs, -48}.width() == x_t{1} >> 48);
static_assert(sut_t{coeffs, -47}.width() == x_t{1} >> 47);
static_assert(sut_t{coeffs, -1}.width() == x_t{1} >> 1);
static_assert(sut_t{coeffs, 0}.width() == x_t{1} << 0);
static_assert(sut_t{coeffs, 1}.width() == x_t{1} << 1);
static_assert(sut_t{coeffs, 13}.width() == x_t{1} << 13);
static_assert(sut_t{coeffs, 14}.width() == x_t{1} << 14);

} // namespace width_tests

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

// coeff[0] packs log2_width into its bottom 8 bits, so underlying must fit in x_t::frac_bits + 7 bits with sign
constexpr auto coeff0_unsafe_bound = coeff_t::value_t{1} << (x_t::frac_bits + 8 - 1 - coeff_t::frac_bits);

TEST(spline_segment, violates_coeff0_positive_packing_bounds)
{
    // this is safe and does not assert
    [[maybe_unused]] auto safe = sut_t({coeff_t{coeff0_unsafe_bound - 1}, coeff_t{0}, coeff_t{0}, coeff_t{0}}, 0);

    // coeff0 is too large to safely shift left by 8 to store coeff0
    EXPECT_DEBUG_DEATH(sut_t({coeff_t{coeff0_unsafe_bound}, coeff_t{0}, coeff_t{0}, coeff_t{0}}, 0), "top 8 bits");
}

TEST(spline_segment, violates_coeff0_negative_packing_bounds)
{
    // this is safe and does not assert
    [[maybe_unused]] auto safe_neg = sut_t({coeff_t{-coeff0_unsafe_bound}, coeff_t{0}, coeff_t{0}, coeff_t{0}}, 0);

    // coeff0 is too large to safely shift left by 8 to store coeff0
    EXPECT_DEBUG_DEATH(sut_t({coeff_t{-coeff0_unsafe_bound - 1}, coeff_t{0}, coeff_t{0}, coeff_t{0}}, 0), "top 8 bits");
}

} // namespace death_tests

#endif

} // namespace segment_test

} // namespace
} // namespace crv::spline
