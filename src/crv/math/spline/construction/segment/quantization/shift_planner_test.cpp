// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "shift_planner.hpp"
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

constexpr auto plan_shift = shift_planner_t{};

// exponents are equal; no relative shift required
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

// negative base shift; the first coefficient itself has a negative shift to start
// base shift (-1) + relative shift (3)
static_assert(plan_shift(-5, -2, -1)
    == shift_planner_t::plan_t{.packed_runtime_shift = 2, .destructive_preshift = 0, .next_accumulator_exponent = -2});

} // namespace
} // namespace crv::spline
