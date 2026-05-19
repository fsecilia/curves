// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment_factory.hpp"
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

// ====================================================================================================================
// quantization
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// shift_planner_t
// --------------------------------------------------------------------------------------------------------------------

namespace shift_planner_tests {

constexpr auto plan_shift = shift_planner_t{};

// exponents are equal; no relative shift required, so just pass through the base t_to_x_shift
static_assert(plan_shift(10, 10, 5)
    == shift_planner_t::plan_t{.packed_runtime_shift = 5, .destructive_preshift = 0, .next_accumulator_exponent = 10});

// next exponent is larger; next term needs a larger runtime shift to align, no destructive preshift needed
// base shift (5) + relative shift (4)
static_assert(plan_shift(10, 14, 5)
    == shift_planner_t::plan_t{.packed_runtime_shift = 9, .destructive_preshift = 0, .next_accumulator_exponent = 14});

// accumulator exponent is larger; next term must be destructively preshifted to align with the accumulator
// base shift (5) + relative shift (-4)
static_assert(plan_shift(14, 10, 5)
    == shift_planner_t::plan_t{.packed_runtime_shift = 5, .destructive_preshift = 4, .next_accumulator_exponent = 14});

// negative domains
// base shift (-1) + relative shift (3)
static_assert(plan_shift(-5, -2, -1)
    == shift_planner_t::plan_t{.packed_runtime_shift = 2, .destructive_preshift = 0, .next_accumulator_exponent = -2});

} // namespace shift_planner_tests

// --------------------------------------------------------------------------------------------------------------------
// mantissa_quantizer_t
// --------------------------------------------------------------------------------------------------------------------

namespace mantissa_quantizer_tests {

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

} // namespace mantissa_quantizer_tests

// --------------------------------------------------------------------------------------------------------------------
// radix_aligner_t
// --------------------------------------------------------------------------------------------------------------------

namespace radix_aligner_tests {

using test_mantissa_t = int32_t;
using test_scaled_int_t = scaled_int_t<test_mantissa_t>;
using test_unpacked_field_t = unpacked_field_t<test_mantissa_t>;

// instantiate the aligner with arbitrary safe bounds for our 32-bit test container
constexpr auto aligner = exponent_aligner_t<-20, 20>{};
constexpr auto align_radix = radix_aligner_t<test_unpacked_field_t, test_scaled_int_t, aligner>{};

// passthrough; exponent is well within the aligner's bounds
// exponent = 5 + 2 = 7, shift output becomes -7, mantissa is untouched
static_assert(align_radix({.mantissa = 100, .exponent = 5}, 2) == test_unpacked_field_t{.mantissa = 100, .shift = -7});

// positive saturation; exponent exceeds max
// exponent = 15 + 10 = 25, clamps to 20
// deficit of 5 means mantissa is left-shifted by 5 (10 << 5 = 320); shift output is -20
static_assert(
    align_radix({.mantissa = 10, .exponent = 15}, 10) == test_unpacked_field_t{.mantissa = 320, .shift = -20});

// negative saturation; exponent falls below min
// exponent = -15 + (-10) = -25, clamps to -20
// surplus of 5 means mantissa is right-shifted by 5 (1000 >> 5 = 31 RNE); shift output is 20
static_assert(
    align_radix({.mantissa = 1000, .exponent = -15}, -10) == test_unpacked_field_t{.mantissa = 31, .shift = 20});

} // namespace radix_aligner_tests

namespace segment_quantizer_tests {

using scalar_t = float_t;
using mantissa_t = int_t;
using unpacked_field_t = unpacked_field_t<mantissa_t>;
using scaled_int_t = scaled_int_t<mantissa_t>;

namespace isolation_tests {

struct float_extractor_t
{
    using scalar_t = scalar_t;

    // mantissa is 10x, exponent is 1x
    constexpr auto operator()(scalar_t scalar) const noexcept -> scaled_int_t
    {
        return {.mantissa = static_cast<mantissa_t>(scalar * 10), .exponent = static_cast<int_t>(scalar)};
    }
};

struct shift_planner_t
{
    constexpr auto operator()(int_t accumulator_exponent, int_t next_exponent, int_t t_to_x_shift) const noexcept
        -> spline::shift_planner_t::plan_t
    {
        return {
            .packed_runtime_shift = t_to_x_shift + accumulator_exponent + next_exponent,
            .destructive_preshift = 0,
            .next_accumulator_exponent = next_exponent,
        };
    }
};

struct mantissa_quantizer_t
{
    static constexpr auto max_container_shift = 31;
    constexpr auto operator()(mantissa_t mantissa, int_t preshift) const noexcept -> mantissa_t
    {
        return static_cast<mantissa_t>(mantissa + preshift);
    }
};

struct radix_aligner_t
{
    using scaled_int_t = scaled_int_t;
    constexpr auto operator()(scaled_int_t const& accum, int_t radix) const noexcept -> unpacked_field_t
    {
        return {.mantissa = static_cast<mantissa_t>(accum.mantissa + radix), .shift = accum.exponent};
    }
};

constexpr auto sut
    = segment_quantizer_t<unpacked_field_t, float_extractor_t, shift_planner_t, mantissa_quantizer_t, radix_aligner_t,
        10, // in_frac_bits
        20, // out_frac_bits
        0 // log2_min_width (t_to_x_shift = 10)
        >{};

// 1.0 -> accum(10, 1)
// 2.0 -> next(20, 2) -> plan(shift: 10+1+2=13, next_exp: 2) -> unpacked[0] = {10, 13}, accum = 20
// 3.0 -> next(30, 3) -> plan(shift: 10+2+3=15, next_exp: 3) -> unpacked[1] = {20, 15}, accum = 30
// 4.0 -> next(40, 4) -> plan(shift: 10+3+4=17, next_exp: 4) -> unpacked[2] = {30, 17}, accum = 40
// align final -> accum(40, 4), out_frac(20) -> unpacked[3] = {60, 4}
static_assert(sut({1.0, 2.0, 3.0, 4.0}, 0)
    == std::array<unpacked_field_t, 4>{unpacked_field_t{.mantissa = 10, .shift = 13},
        unpacked_field_t{.mantissa = 20, .shift = 15}, unpacked_field_t{.mantissa = 30, .shift = 17},
        unpacked_field_t{.mantissa = 60, .shift = 4}});

} // namespace isolation_tests

namespace end_to_end_test {

using x_t = fixed_t<int64_t, 14>;
using y_t = fixed_t<int64_t, 25>;
using cubic_t = std::array<scalar_t, 4>;
using unpacked_segment_t = std::array<unpacked_field_t, fields_per_segment>;

constexpr auto aligner = exponent_aligner_t<-30, 30>{};
constexpr auto sut = segment_quantizer_t<unpacked_field_t, float_extractor_t<scalar_t>, shift_planner_t,
    mantissa_quantizer_t<mantissa_t>, radix_aligner_t<unpacked_field_t, scaled_int_t, aligner>,
    x_t::frac_bits, // in_frac_bits
    y_t::frac_bits, // out_frac_bits
    -8 // log2_min_width
    >{};

// y = 0.125x^3 + 0.25x^2 + 0.5x + 1.0
//
// Each coefficient here has the same mantissa, but in the packed format, the shifts cancel and the mantissas are
// preserved.
static_assert(sut({0.125, 0.25, 0.5, 1.0}, 0)
    == unpacked_segment_t{
        unpacked_field_t{.mantissa = 4503599627370496, .shift = 15},
        unpacked_field_t{.mantissa = 4503599627370496, .shift = 15},
        unpacked_field_t{.mantissa = 4503599627370496, .shift = 15},
        unpacked_field_t{.mantissa = 4503599627370496, .shift = 27},
    });

// y = 1.0x^3 + 0.5x^2 + 0.25x + 0.125
//
// Each coefficient here starts with the same mantissa, but in the packed format, each must be shifted destructively to
// prevent left shifts during evaluation.
//
// float extraction:
//    1.0 is exactly 2^52 * 2^-52.
//    initial accumulator: mantissa = 4503599627370496 (2^52), exponent = -52
//    next term (0.5): exponent is -53
// iteration 1 (c3 and c2):
//    relative_shift = next_exponent - accumulator_exponent = -53 - (-52) = -1
//    destructive_preshift = max(0, 1) = 1
//    0.5 mantissa is shifted right by 1 -> 2251799813685248
//    packed_runtime_shift = t_to_x_shift (14) + max(0, -1) = 14
//    accumulator_exponent remains -52
// iteration 2 & 3:
//    Follows the same pattern, destructively shifting the next mantissa by 2 and 3 respectively, yielding the halved
//    mantissas
// final radix alignment:
//    accumulator_exponent is still -52, out_frac_bits is 25
//    target_exponent = -52 + 25 = -27
//    -27 is within the exponent_aligner_t bounds [-30, 30], so left_shift is 0
//    output shift = -(-27) = 27
static_assert(sut({1.0, 0.5, 0.25, 0.125}, 0)
    == unpacked_segment_t{
        unpacked_field_t{.mantissa = 4503599627370496, .shift = 14},
        unpacked_field_t{.mantissa = 2251799813685248, .shift = 14},
        unpacked_field_t{.mantissa = 1125899906842624, .shift = 14},
        unpacked_field_t{.mantissa = 562949953421312, .shift = 27},
    });

// destructive flushing
//
// c2 is so small relative to c3 that the required destructive preshift exceeds the 64-bit container size, causing it to
// be flushed to zero.
static_assert(sut({1.0, 1.2e-35, 1.2e-35, 1.2e-35}, 0)
    == unpacked_segment_t{
        unpacked_field_t{.mantissa = 4503599627370496, .shift = 14},
        unpacked_field_t{.mantissa = 0, .shift = 14},
        unpacked_field_t{.mantissa = 0, .shift = 14},
        unpacked_field_t{.mantissa = 0, .shift = 27},
    });

} // namespace end_to_end_test

} // namespace segment_quantizer_tests

// ====================================================================================================================
// packing
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// field_packer_t
// --------------------------------------------------------------------------------------------------------------------

namespace field_packer_tests {

using traits_t = traits_t<unpacked_field_t<int_t>>;

using packed_field_t = traits_t::packed_field_t;
using unpacked_field_t = traits_t::unpacked_field_t;
using mantissa_t = traits_t::mantissa_t;

using field_layout_t = field_layout_t<packed_field_t>;
using field_packer_t = field_packer_t<packed_field_t>;

constexpr auto layout = field_layout_t{.shift_width = 4, .is_signed = true};
constexpr auto pack_field = field_packer_t{};

// 60-bit signed mantissa bounds
constexpr auto max_mantissa = mantissa_t{(1LL << 59) - 1};
constexpr auto min_mantissa = mantissa_t{-(1LL << 59)};

// 4-bit signed shift bounds
constexpr auto max_shift = int_t{7};
constexpr auto min_shift = int_t{-8};

// max mantissa, min shift
static_assert(pack_field(unpacked_field_t{.mantissa = max_mantissa, .shift = min_shift}, layout)
    == packed_field_t{0x7FFFFFFFFFFFFFF8});

// min mantissa, min shift
static_assert(pack_field(unpacked_field_t{.mantissa = min_mantissa, .shift = min_shift}, layout)
    == packed_field_t{0x8000000000000008});

// max mantissa, max shift
static_assert(pack_field(unpacked_field_t{.mantissa = max_mantissa, .shift = max_shift}, layout)
    == packed_field_t{0x7FFFFFFFFFFFFFF7});

// min mantissa, max shift
static_assert(pack_field(unpacked_field_t{.mantissa = min_mantissa, .shift = max_shift}, layout)
    == packed_field_t{0x8000000000000007});

// zero
static_assert(pack_field(unpacked_field_t{.mantissa = 0, .shift = 0}, layout) == packed_field_t{0});

} // namespace field_packer_tests

// --------------------------------------------------------------------------------------------------------------------
// segment_packer_t
// --------------------------------------------------------------------------------------------------------------------

namespace segment_packer_tests {

using field_layout_t = int_t;
struct segment_layout_t
{
    field_layout_t intermediate;
    field_layout_t final;
};
constexpr auto segment_layout = segment_layout_t{.intermediate = 3, .final = 5};

using unpacked_field_t = int_t;
using unpacked_segment_t = std::array<unpacked_field_t, fields_per_segment>;

struct packed_field_t
{
    unpacked_field_t packed_field;
    field_layout_t field_layout;

    auto operator==(packed_field_t const&) const noexcept -> bool = default;
};
using packed_segment_t = std::array<packed_field_t, fields_per_segment>;

struct field_packer_t
{
    using packed_field_t = packed_field_t;

    constexpr auto operator()(unpacked_field_t packed_field, field_layout_t field_layout) const noexcept
        -> packed_field_t
    {
        return packed_field_t{.packed_field = packed_field, .field_layout = field_layout};
    }
};

constexpr auto pack_segment = segment_packer_t<packed_segment_t, unpacked_segment_t, field_packer_t, segment_layout>{};

static_assert(pack_segment(unpacked_segment_t{7, 11, 13, 17})
    == packed_segment_t{
        packed_field_t{.packed_field = 7, .field_layout = 3},
        packed_field_t{.packed_field = 11, .field_layout = 3},
        packed_field_t{.packed_field = 13, .field_layout = 3},
        packed_field_t{.packed_field = 17, .field_layout = 5},
    });

} // namespace segment_packer_tests

// ====================================================================================================================
// orchestration
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// segment_factory_t
// --------------------------------------------------------------------------------------------------------------------

namespace segment_factory_tests {

struct cubic_t
{
    int_t id;
    constexpr auto operator==(cubic_t const&) const noexcept -> bool = default;
};

struct unpacked_segment_t
{
    cubic_t cubic;
    int_t log2_width;
    constexpr auto operator==(unpacked_segment_t const&) const noexcept -> bool = default;
};

struct packed_segment_t
{
    unpacked_segment_t unpacked;
    constexpr auto operator==(packed_segment_t const&) const noexcept -> bool = default;
};

struct segment_t
{
    packed_segment_t packed_segment;

    constexpr explicit segment_t(packed_segment_t packed) noexcept : packed_segment{packed} {}
    constexpr auto operator==(segment_t const&) const noexcept -> bool = default;
};

struct segment_quantizer_t
{
    using cubic_t = cubic_t;
    constexpr auto operator()(cubic_t const& cubic, int_t log2_width) const noexcept -> unpacked_segment_t
    {
        return unpacked_segment_t{.cubic = cubic, .log2_width = log2_width};
    }
};

struct segment_packer_t
{
    constexpr auto operator()(unpacked_segment_t const& unpacked) const noexcept -> packed_segment_t
    {
        return packed_segment_t{.unpacked = unpacked};
    }
};

constexpr auto build_segment = segment_factory_t<segment_t, segment_quantizer_t, segment_packer_t>{};

// verify the factory correctly delegates to the quantizer, then the packer, then wraps the result
static_assert(build_segment(cubic_t{.id = 42}, 8)
    == segment_t{packed_segment_t{unpacked_segment_t{.cubic = cubic_t{.id = 42}, .log2_width = 8}}});

} // namespace segment_factory_tests

} // namespace
} // namespace crv::spline
