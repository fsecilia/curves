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

// exponents are equal; no relative shift required
static_assert(plan_shift(1, 10, 10, 5)
    == plan_t{
        .packed_runtime_shift = 10 - 10 + 5,
        .destructive_preshift = 0,
        .next_accumulator_exponent = 10,
    });

// next exponent is larger; next term needs a larger runtime shift to align, no destructive preshift needed
// base shift (5) + relative shift (4)
static_assert(plan_shift(1, 10, 14, 5)
    == plan_t{
        .packed_runtime_shift = 14 - 10 + 5,
        .destructive_preshift = 0,
        .next_accumulator_exponent = 14,
    });

// accumulator exponent is larger, but shift can be absorbed
// base shift (5) + relative shift (-4)
static_assert(plan_shift(1, 14, 10, 5)
    == plan_t{
        .packed_runtime_shift = 10 - 14 + 5,
        .destructive_preshift = 0,
        .next_accumulator_exponent = 10,
    });

// max shift that can be absorbed
static_assert(plan_shift(1, 14, 10, 4)
    == plan_t{
        .packed_runtime_shift = 10 - 14 + 4,
        .destructive_preshift = 0,
        .next_accumulator_exponent = 10,
    });

// first destructive shift
static_assert(plan_shift(1, 14, 10, 3)
    == plan_t{
        .packed_runtime_shift = 0,
        .destructive_preshift = 14 - 10 - 3,
        .next_accumulator_exponent = 14 - 3,
    });

// destructive negative shift
static_assert(plan_shift(1, 14, 10, -1)
    == plan_t{
        .packed_runtime_shift = 0,
        .destructive_preshift = 14 - 10 + 1,
        .next_accumulator_exponent = 14 + 1,
    });

// negative base shift; the first coefficient itself has a negative shift to start
// base shift (-1) + relative shift (3)
static_assert(plan_shift(1, -5, -2, -1)
    == plan_t{.packed_runtime_shift = 2, .destructive_preshift = 0, .next_accumulator_exponent = -2});

// overflow
//
// Accumulator carries 62 bits of magnitude. t_to_x is 10. It requires a minimum right-shift of 10 to stay within
// max_safe_bits (62) in the wide product. The ideal runtime shift is only 5 (10 + (-5)), so the guard clamps the
// runtime shift to 10 and destructively preshifts the next term by the shortfall (5).
static_assert(plan_shift(62, 20, 15, 10)
    == plan_t{
        .packed_runtime_shift = 10,
        .destructive_preshift = 5,
        .next_accumulator_exponent = 15 + 5,
    });

// upper-bound: post-sum accumulator with 63 bits is the widest the planner will accept
//
// At runtime a sum of two max_safe_bits-sized operands can produce 63 bits. The planner must still pick a safe shift
// that brings the next aligned_product back within max_safe_bits (62). With t_to_x = 10 and equal exponents, the ideal
// shift is 10 but the safe floor is 11; the shortfall of 1 is absorbed by destructively preshifting the next term.
static_assert(plan_shift(63, 0, 0, 10)
    == plan_t{
        .packed_runtime_shift = 11,
        .destructive_preshift = 1,
        .next_accumulator_exponent = 1,
    });

} // namespace
} // namespace crv::spline
