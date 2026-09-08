// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment_quantizer.hpp"
#include <crv/math/jet/jet.hpp>
#include <crv/math/limits.hpp>
#include <crv/spline/construction/segment/local_coordinate.hpp>
#include <crv/spline/construction/segment/shift_planner.hpp>
#include <crv/test/test.hpp>
#include <cmath>
#include <limits>

namespace crv::spline {
namespace {

//
// significand_quantizer_t
//

namespace significand_quantizer_tests {

using significand_t = int32_t;
constexpr auto quantize = significand_quantizer_t<significand_t>{};
constexpr auto truncating_quantize = significand_quantizer_t<significand_t, rounding_modes::shr::truncate>{};

// passthrough with no shift
static_assert(quantize(100, 0) == 100);
static_assert(quantize(3, 0) == 3);
static_assert(quantize(-3, 0) == -3);

// basic shifting
static_assert(quantize(100, 2) == 25);
static_assert(quantize(-100, 2) == -25);
static_assert(truncating_quantize(3, 1) == 1);

// rne boundary conditions
static_assert(quantize(3, 1) == 2); // 1.5 rounds up to 2
static_assert(quantize(5, 1) == 2); // 2.5 rounds down to 2
static_assert(quantize(7, 1) == 4); // 3.5 rounds up to 4
static_assert(quantize(-3, 1) == -2); // -1.5 rounds to -2
static_assert(quantize(-5, 1) == -2); // -2.5 rounds to -2

// container shift saturation; max_container_shift for int32_t is 31
static_assert(quantize(max<significand_t>(), 30) == (max<significand_t>() >> 30) + 1); // no flush before max
static_assert(quantize(max<significand_t>(), 31) == 0); // flush exactly at max
static_assert(quantize(max<significand_t>(), 32) == 0); // flush exceeding max
static_assert(quantize(max<significand_t>(), 100) == 0); // flush large values

} // namespace significand_quantizer_tests

//
// radix_aligner_t
//

namespace radix_aligner_tests {

using significand_t = int32_t;
using scaled_int_t = scaled_int_t<significand_t>;

struct unpacked_field_t
{
    significand_t significand;
    int_t shift;
    constexpr auto operator==(unpacked_field_t const&) const noexcept -> bool = default;
};

// instantiate the aligner with arbitrary safe bounds for the 32-bit test container
constexpr auto aligner = exponent_aligner_t<-20, 20>{};
constexpr auto truncating_aligner = exponent_aligner_t<-20, 20, rounding_modes::shr::truncate>{};
constexpr auto align_radix = radix_aligner_t<unpacked_field_t, scaled_int_t, aligner>{};

// passthrough; exponent is well within the aligner's bounds
// exponent = 5 + 2 = 7, shift output becomes -7, significand is untouched
static_assert(align_radix({.significand = 100, .exponent = 5}, 2) == unpacked_field_t{.significand = 100, .shift = -7});

// rounding mode controls right shifts
static_assert(truncating_aligner(scaled_int_t{.significand = 3, .exponent = -21})
    == scaled_int_t{.significand = 1, .exponent = -20});

// positive saturation; exponent exceeds max
// exponent = 15 + 10 = 25, clamps to 20
// deficit of 5 means significand is left-shifted by 5 (10 << 5 = 320); shift output is -20
static_assert(
    align_radix({.significand = 10, .exponent = 15}, 10) == unpacked_field_t{.significand = 320, .shift = -20});

// negative saturation; exponent falls below min
// exponent = -15 + (-10) = -25, clamps to -20
// surplus of 5 means significand is right-shifted by 5 (1000 >> 5 = 31 RNE); shift output is 20
static_assert(
    align_radix({.significand = 1000, .exponent = -15}, -10) == unpacked_field_t{.significand = 31, .shift = 20});

} // namespace radix_aligner_tests

//
// segment_quantizer_t
//

namespace segment_quantizer_tests {

using scalar_t = float_t;
using significand_t = int_t;
using unpacked_field_t = spline::unpacked_field_t<significand_t>;
using scaled_int_t = crv::scaled_int_t<significand_t>;

auto const max_intermediate_shift = 0x7f;

namespace isolation_tests {

using x_t = fixed_t<int64_t, 10>;
using y_t = fixed_t<int64_t, 20>;
using unpacked_segment_t = spline::unpacked_segment_t<unpacked_field_t, y_t>;

struct float_extractor_t
{
    using scalar_t = float_t;

    // significand is 10x, exponent is 1x
    constexpr auto operator()(scalar_t scalar) const noexcept -> scaled_int_t
    {
        return {.significand = static_cast<significand_t>(scalar * 10), .exponent = static_cast<int_t>(scalar)};
    }
};

struct shift_planner_t
{
    using plan_t = spline::shift_planner_t<significand_t>::plan_t;

    constexpr auto operator()(int_t accumulator_bit_count, int_t accumulator_exponent, int_t next_exponent,
        int_t coordinate_radix_shift, int_t coordinate_magnitude_bits) const noexcept -> plan_t
    {
        return {
            .packed_runtime_shift = coordinate_radix_shift + coordinate_magnitude_bits + accumulator_exponent
                + next_exponent + accumulator_bit_count,
            .destructive_preshift = 0,
            .next_accumulator_exponent = next_exponent,
        };
    }
};

struct significand_quantizer_t
{
    constexpr auto operator()(significand_t significand, int_t preshift) const noexcept -> significand_t
    {
        return static_cast<significand_t>(significand + preshift);
    }
};

struct radix_aligner_t
{
    using scaled_int_t = crv::scaled_int_t<significand_t>;

    constexpr auto operator()(scaled_int_t const& accum, int_t radix) const noexcept -> unpacked_field_t
    {
        return {.significand = static_cast<significand_t>(accum.significand + radix), .shift = accum.exponent};
    }
};

constexpr auto sut = segment_quantizer_t<unpacked_segment_t, float_extractor_t, shift_planner_t,
    significand_quantizer_t, radix_aligner_t, max_intermediate_shift, x_t>{};

// Project cubic order is {d, c, b, a}. Width raw=5 contributes three magnitude bits.
//
// d=1 -> accum(10, 1), bit_count=4
// c=2 -> shift=10+3+1+2+4=20 -> d={10,20}, accum=(20,2), bit_count=5
// b=3 -> shift=10+3+2+3+5=23 -> c={20,23}, accum=(30,3)
// final y-radix alignment -> b={30+20,3}
// a=4 never enters that recurrence; with x0=1 it becomes plain y_t g0=4.
TEST(segment_quantizer_isolation_tests, transfer_constant_does_not_participate_in_dynamic_shift_planning)
{
    auto const expected = unpacked_segment_t{
        .d = {.significand = 10, .shift = 20},
        .c = {.significand = 20, .shift = 23},
        .b = {.significand = 50, .shift = 3},
        .g0 = y_t{4},
    };
    EXPECT_EQ(sut({1.0, 2.0, 3.0, 4.0}, 3.0, x_t::literal(5), x_t{1}), expected);

    auto const small_a = sut({1.0, 2.0, 3.0, 0.25}, 3.0, x_t::literal(5), x_t{1});
    auto const large_a = sut({1.0, 2.0, 3.0, 64.0}, 3.0, x_t::literal(5), x_t{1});
    EXPECT_EQ(small_a.d, large_a.d);
    EXPECT_EQ(small_a.c, large_a.c);
    EXPECT_EQ(small_a.b, large_a.b);
    EXPECT_EQ(small_a.g0, to_fixed<y_t>(0.25));
    EXPECT_EQ(large_a.g0, y_t{64});
}

TEST(segment_quantizer_isolation_tests, authoritative_derivative_drives_c_to_b_planning_and_final_b)
{
    auto const expected = unpacked_segment_t{
        .d = {.significand = 10, .shift = 20},
        .c = {.significand = 20, .shift = 27},
        .b = {.significand = 90, .shift = 7},
        .g0 = y_t{4},
    };

    EXPECT_EQ(sut({1.0, 2.0, 3.0, 4.0}, 7.0, x_t::literal(5), x_t{1}), expected);
}

// at the global origin g0 is unused and stored as zero; the transfer cubic must itself satisfy T(0)=0
constexpr auto const first = sut({1.0, 2.0, 3.0, 0.0}, 3.0, x_t::literal(5), x_t{0});
static_assert(first.g0 == y_t{0});

} // namespace isolation_tests

namespace end_to_end_tests {

using x_t = fixed_t<int64_t, 14>;
using y_t = fixed_t<int64_t, 25>;
using unpacked_segment_t = spline::unpacked_segment_t<unpacked_field_t, y_t>;

constexpr auto aligner = exponent_aligner_t<-30, 30>{};
constexpr auto sut = segment_quantizer_t<unpacked_segment_t, float_extractor_t<scalar_t>,
    shift_planner_t<significand_t>, significand_quantizer_t<significand_t>,
    radix_aligner_t<unpacked_field_t, scaled_int_t, aligner>, max_intermediate_shift, x_t>{};

TEST(segment_quantizer_end_to_end_tests, gain_anchor_precondition_uses_actual_fixed_conversion_source)
{
    auto const x0 = x_t{2};
    auto const scalar_x0 = from_fixed<scalar_t>(x0);
    auto const first_unrepresentable
        = std::ldexp(scalar_t{1}, std::numeric_limits<typename y_t::value_t>::digits - y_t::frac_bits);
    auto const last_representable = std::nextafter(first_unrepresentable, scalar_t{0});
    auto const representable = cubic_t<scalar_t>{0.0, 0.0, 0.0, last_representable * scalar_x0};
    auto const unrepresentable = cubic_t<scalar_t>{0.0, 0.0, 0.0, first_unrepresentable * scalar_x0};

    EXPECT_TRUE(sut.is_g0_representable(representable, x0));
    EXPECT_EQ(sut(representable, 0.0, x_t{1}, x0).g0, to_fixed<y_t>(last_representable));
    EXPECT_FALSE(sut.is_g0_representable(unrepresentable, x0));
}

TEST(segment_quantizer_end_to_end_tests, global_origin_does_not_require_gain_anchor_representability)
{
    EXPECT_TRUE(sut.is_g0_representable({0.0, 0.0, 0.0, 1e300}, x_t{0}));
}

// The three S coefficients maintain a high-precision dynamic representation. g0 is directly quantized from the
// floating Hermite endpoint a and the actual fixed runtime origin converted back to scalar.
TEST(segment_quantizer_end_to_end_tests, quantizes_s_and_g0_in_their_distinct_representations)
{
    auto const segment = sut({0.125, 0.25, 0.5, 3.0}, 0.5, x_t{1}, x_t{2});
    EXPECT_EQ(segment.d, (unpacked_field_t{.significand = 4503599627370496, .shift = 15}));
    EXPECT_EQ(segment.c, (unpacked_field_t{.significand = 4503599627370496, .shift = 15}));
    EXPECT_EQ(segment.b, (unpacked_field_t{.significand = 4503599627370496, .shift = 28}));
    EXPECT_EQ(segment.g0, to_fixed<y_t>(1.5));
}

TEST(segment_quantizer_end_to_end_tests, transfer_constant_cannot_cause_destructive_dynamic_flushing)
{
    auto const flushed = sut({1.0, 1.2e-35, 0.25, 7.0}, 0.25, x_t{1}, x_t{2});
    auto const flushed_other_a = sut({1.0, 1.2e-35, 0.25, 0.125}, 0.25, x_t{1}, x_t{2});
    EXPECT_EQ(flushed.d, flushed_other_a.d);
    EXPECT_EQ(flushed.c, flushed_other_a.c);
    EXPECT_EQ(flushed.b, flushed_other_a.b);
    EXPECT_NE(flushed.g0, flushed_other_a.g0);
}

TEST(segment_quantizer_end_to_end_tests, preserves_left_derivative_across_normalized_hermite_round_trip)
{
    constexpr auto left_endpoint_derivative = scalar_t{0.3};
    constexpr auto width = scalar_t{3.0};
    constexpr auto normalized = hermite_converter_t<scalar_t>{}(
        jet_t<scalar_t>{0.0, left_endpoint_derivative * width}, jet_t<scalar_t>{1.0, 0.0});
    constexpr auto local = local_coordinate_converter_t<scalar_t>{}(normalized, width);
    static_assert(local[2] != left_endpoint_derivative);

    auto const authoritative = sut(local, left_endpoint_derivative, x_t{3}, x_t{0});
    auto const round_tripped = sut(local, local[2], x_t{3}, x_t{0});
    EXPECT_NE(authoritative.b, round_tripped.b);
}

} // namespace end_to_end_tests
} // namespace segment_quantizer_tests

} // namespace
} // namespace crv::spline
