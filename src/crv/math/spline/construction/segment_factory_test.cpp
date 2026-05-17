// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "segment_factory.hpp"
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

// ====================================================================================================================
// shift_planner_t
// ====================================================================================================================

namespace shift_planner_tests {

constexpr auto plan_shift = shift_planner_t{};

// exponents are equal; no relative shift required, so just pass through the base t_to_x_shift
static_assert(plan_shift(10, 10, 5)
    == shift_planner_t::plan_t{.packed_runtime_shift = 5, .destructive_preshift = 0, .next_accum_exponent = 10});

// next exponent is larger; next term needs a larger runtime shift to align, no destructive preshift needed
// base shift (5) + relative shift (4)
static_assert(plan_shift(10, 14, 5)
    == shift_planner_t::plan_t{.packed_runtime_shift = 9, .destructive_preshift = 0, .next_accum_exponent = 14});

// accumulator exponent is larger; next term must be destructively preshifted to align with the accumulator
// base shift (5) + relative shift (-4)
static_assert(plan_shift(14, 10, 5)
    == shift_planner_t::plan_t{.packed_runtime_shift = 5, .destructive_preshift = 4, .next_accum_exponent = 14});

// negative domains
// base shift (-1) + relative shift (3)
static_assert(plan_shift(-5, -2, -1)
    == shift_planner_t::plan_t{.packed_runtime_shift = 2, .destructive_preshift = 0, .next_accum_exponent = -2});

} // namespace shift_planner_tests

// ====================================================================================================================
// mantissa_quantizer_t
// ====================================================================================================================

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

// ====================================================================================================================
// radix_aligner_t
// ====================================================================================================================

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

// negative Saturation; exponent falls below min
// exponent = -15 + (-10) = -25, clamps to -20
// surplus of 5 means mantissa is right-shifted by 5 (1000 >> 5 = 31 RNE); shift output is 20
static_assert(
    align_radix({.mantissa = 1000, .exponent = -15}, -10) == test_unpacked_field_t{.mantissa = 31, .shift = 20});

} // namespace radix_aligner_tests

// ====================================================================================================================
// field_packer_t
// ====================================================================================================================

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

} // namespace
} // namespace crv::spline
