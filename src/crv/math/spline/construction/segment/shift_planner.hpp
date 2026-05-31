// SPDX-License-Identifier: MIT

/// \file
/// \brief determines dynamic shifts during evaluation
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/int_traits.hpp>
#include <climits>

namespace crv::spline {

/// determines safe relative shifts to align radices between products and the next coefficient during evaluation
///
/// The evaluator only shifts right. This type determines if a simple right shift is sufficient, how much to shift, and
/// how much to destructively preshift the mantissa when a left shift is required.
///
/// The planner reasons purely in bit counts. The caller is responsible for tracking the runtime accumulator's bit
/// count, which is not the bit count of any single coefficient: at runtime, the accumulator is a partial sum
/// (aligned_product + coefficient), which can be 1 bit wider than either operand. Passing the next-coefficient mantissa
/// bit count here would understate the actual accumulator width and undersize the shift.
template <signed_integral t_mantissa_t> struct shift_planner_t
{
    using mantissa_t = t_mantissa_t;

    /// largest accumulator bit count planner can model
    ///
    /// This is the bit width minus the sign bit, the signed magnitude bits.
    static constexpr auto max_accumulator_bit_count = static_cast<int_t>(sizeof(mantissa_t) * CHAR_BIT) - 1;

    /// largest safe accumulator bit count when summing
    ///
    /// This leaves room for a carry without wrapping.
    static constexpr auto max_safe_bits = max_accumulator_bit_count - 1;

    struct plan_t
    {
        int_t packed_runtime_shift; ///< relative shift performed at runtime during evaluation
        int_t destructive_preshift; ///< shift applied to next coefficient before packing
        int_t next_accumulator_exponent; ///< state carried to next planning step

        auto operator==(plan_t const&) const noexcept -> bool = default;
    };

    /// plan runtime right-shift for one evaluation step
    ///
    /// \param accumulator_bit_count bit-count upper bound on the runtime accumulator entering the next multiplication.
    /// 0 signals the accumulator is exactly zero and no bits are used.
    constexpr auto operator()(int_t accumulator_bit_count, int_t accumulator_exponent, int_t next_exponent,
        int_t t_to_x_shift) const noexcept -> plan_t
    {
        assert(accumulator_bit_count >= 0 && accumulator_bit_count <= max_accumulator_bit_count
            && "shift_planner_t: accumulator_bit_count must fit in narrow_t");

        // determine min shift to keep result in range
        //
        // Multiplying by x at runtime adds up to `t_to_x_shift` bits of magnitude. This is the shift floor to keep the
        // result <= max_safe_bits.
        auto const min_safe_shift = max<int_t>(0, accumulator_bit_count + t_to_x_shift - max_safe_bits);

        auto const relative_shift = next_exponent - accumulator_exponent;
        auto const ideal_runtime_shift = t_to_x_shift + relative_shift;

        auto const plan = ideal_runtime_shift >= min_safe_shift
            // dynamic shift has room to absorb mantissa's exponent
            ? plan_t{
                .packed_runtime_shift = ideal_runtime_shift,
                .destructive_preshift = 0,
                .next_accumulator_exponent = next_exponent,
            }
            // exponent is larger than dynamic shift can absorb; destructively right-shift mantissa
            : plan_t{
                .packed_runtime_shift = min_safe_shift,
                .destructive_preshift = min_safe_shift - ideal_runtime_shift,
                .next_accumulator_exponent = next_exponent + (min_safe_shift - ideal_runtime_shift),
            };

        // after shifting, aligned_product magnitude is at most max_safe_bits, leaving headroom for sum
        assert((accumulator_bit_count == 0
                   || accumulator_bit_count + t_to_x_shift - plan.packed_runtime_shift <= max_safe_bits)
            && "shift_planner_t: planned shift fails aligned_product invariant");

        return plan;
    }
};

} // namespace crv::spline
