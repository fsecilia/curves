// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "shift_planner.hpp"
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

using mantissa_t = int64_t;
constexpr auto plan_shift = shift_planner_t<mantissa_t>{};
using plan_t = shift_planner_t<mantissa_t>::plan_t;

// equal exponents: semantic radix alone determines alignment
static_assert(plan_shift(1, 10, 10, 5, 2)
    == plan_t{.packed_runtime_shift = 5, .destructive_preshift = 0, .next_accumulator_exponent = 10});

// coefficient exponent alignment uses coordinate radix, not coordinate magnitude
static_assert(plan_shift(1, 10, 14, 5, 20)
    == plan_t{.packed_runtime_shift = 9, .destructive_preshift = 0, .next_accumulator_exponent = 14});
static_assert(plan_shift(1, 14, 10, 5, 20)
    == plan_t{.packed_runtime_shift = 1, .destructive_preshift = 0, .next_accumulator_exponent = 10});

// if ideal alignment would require a left shift, preserve a right-only runtime evaluator by preshifting the next term
static_assert(plan_shift(1, 14, 10, 3, 1)
    == plan_t{.packed_runtime_shift = 0, .destructive_preshift = 1, .next_accumulator_exponent = 11});

// width magnitude is an independent safety floor
//
// Here radix alignment wants shift 5, but a 10-bit coordinate bound forces shift 10 to leave carry headroom.
static_assert(plan_shift(62, 20, 15, 10, 10)
    == plan_t{.packed_runtime_shift = 10, .destructive_preshift = 5, .next_accumulator_exponent = 20});

// changing only the width magnitude can force extra safety shift without changing semantic radix compensation
static_assert(plan_shift(60, 0, 0, 14, 2)
    == plan_t{.packed_runtime_shift = 14, .destructive_preshift = 0, .next_accumulator_exponent = 0});
static_assert(plan_shift(60, 0, 0, 14, 20)
    == plan_t{.packed_runtime_shift = 18, .destructive_preshift = 4, .next_accumulator_exponent = 4});

// widest post-sum accumulator remains modelled safely
static_assert(plan_shift(63, 0, 0, 10, 10)
    == plan_t{.packed_runtime_shift = 11, .destructive_preshift = 1, .next_accumulator_exponent = 1});

} // namespace
} // namespace crv::spline
