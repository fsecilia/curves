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
