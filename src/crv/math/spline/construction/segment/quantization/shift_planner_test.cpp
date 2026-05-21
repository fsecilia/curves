// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "shift_planner.hpp"
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

constexpr auto plan_shift = shift_planner_t{};

// exponents are equal; no relative shift required
static_assert(plan_shift(1, 10, 10, 5)
    == shift_planner_t::plan_t{
        .packed_runtime_shift = 10 - 10 + 5,
        .destructive_preshift = 0,
        .next_accumulator_exponent = 10,
    });

// next exponent is larger; next term needs a larger runtime shift to align, no destructive preshift needed
// base shift (5) + relative shift (4)
static_assert(plan_shift(1, 10, 14, 5)
    == shift_planner_t::plan_t{
        .packed_runtime_shift = 14 - 10 + 5,
        .destructive_preshift = 0,
        .next_accumulator_exponent = 14,
    });

// accumulator exponent is larger, but shift can be absorbed
// base shift (5) + relative shift (-4)
static_assert(plan_shift(1, 14, 10, 5)
    == shift_planner_t::plan_t{
        .packed_runtime_shift = 10 - 14 + 5,
        .destructive_preshift = 0,
        .next_accumulator_exponent = 10,
    });

// max shift that can be absorbed
static_assert(plan_shift(1, 14, 10, 4)
    == shift_planner_t::plan_t{
        .packed_runtime_shift = 10 - 14 + 4,
        .destructive_preshift = 0,
        .next_accumulator_exponent = 10,
    });

// first destructive shift
static_assert(plan_shift(1, 14, 10, 3)
    == shift_planner_t::plan_t{
        .packed_runtime_shift = 0,
        .destructive_preshift = 14 - 10 - 3,
        .next_accumulator_exponent = 14 - 3,
    });

// destructive negative shift
static_assert(plan_shift(1, 14, 10, -1)
    == shift_planner_t::plan_t{
        .packed_runtime_shift = 0,
        .destructive_preshift = 14 - 10 + 1,
        .next_accumulator_exponent = 14 + 1,
    });

// negative base shift; the first coefficient itself has a negative shift to start
// base shift (-1) + relative shift (3)
static_assert(plan_shift(1, -5, -2, -1)
    == shift_planner_t::plan_t{.packed_runtime_shift = 2, .destructive_preshift = 0, .next_accumulator_exponent = -2});

// overflow
//
// Accumulator is massive with 62 bits of magnitude. t_to_x is 10. It requires a minimum right-shift of 10 to stay
// within 62 bits in the wide product. The ideal runtime shift is only 5 (10 + (-5)), so the guard clamps the runtime
// shift to 10 and destructively preshifts the next term by the shortfall (5).
static_assert(plan_shift((1LL << 61), 20, 15, 10)
    == shift_planner_t::plan_t{
        .packed_runtime_shift = 10,
        .destructive_preshift = 5,
        .next_accumulator_exponent = 15 + 5,
    });

} // namespace
} // namespace crv::spline
