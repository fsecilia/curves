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

/// plans right shifts between Horner terms
///
/// Runtime only shifts right. If radix alignment would need a left shift, the planner preshifts the next coefficient
/// instead. It works from accumulator bit counts because a partial sum can be one bit wider than either input.
///
/// coordinate_radix_shift describes fixed-point radix alignment. coordinate_magnitude_bits is separate and only bounds
/// product growth from the interval width.
template <signed_integral t_mantissa_t> struct shift_planner_t
{
    using mantissa_t = t_mantissa_t;

    /// largest accumulator magnitude the planner can model
    static constexpr auto max_accumulator_bit_count = static_cast<int_t>(sizeof(mantissa_t) * CHAR_BIT) - 1;

    /// largest accumulator magnitude that still leaves one carry bit
    static constexpr auto max_safe_bits = max_accumulator_bit_count - 1;

    struct plan_t
    {
        int_t packed_runtime_shift; ///< relative shift performed at runtime during evaluation
        int_t destructive_preshift; ///< shift applied to next coefficient before packing
        int_t next_accumulator_exponent; ///< state carried to next planning step

        auto operator==(plan_t const&) const noexcept -> bool = default;
    };

    /// plans one runtime right shift
    ///
    /// \param accumulator_bit_count upper bound on accumulator magnitude bits; 0 means exact zero
    constexpr auto operator()(int_t accumulator_bit_count, int_t accumulator_exponent, int_t next_exponent,
        int_t coordinate_radix_shift, int_t coordinate_magnitude_bits) const noexcept -> plan_t
    {
        assert(accumulator_bit_count >= 0 && accumulator_bit_count <= max_accumulator_bit_count
            && "shift_planner_t: accumulator bit count exceeds mantissa magnitude bits");
        assert(coordinate_radix_shift >= 0 && "shift_planner_t: coordinate radix shift must be nonnegative");
        assert(coordinate_magnitude_bits >= 0 && "shift_planner_t: coordinate magnitude bits must be nonnegative");

        // leave one carry bit after multiplying by u
        auto const min_safe_shift = max<int_t>(0, accumulator_bit_count + coordinate_magnitude_bits - max_safe_bits);

        auto const relative_shift = next_exponent - accumulator_exponent;
        auto const ideal_runtime_shift = coordinate_radix_shift + relative_shift;

        auto const plan = ideal_runtime_shift >= min_safe_shift
            // runtime shift can absorb coefficient exponent
            ? plan_t{
                .packed_runtime_shift = ideal_runtime_shift,
                .destructive_preshift = 0,
                .next_accumulator_exponent = next_exponent,
            }
            // preshift coefficient when runtime alignment cannot absorb its exponent
            : plan_t{
                .packed_runtime_shift = min_safe_shift,
                .destructive_preshift = min_safe_shift - ideal_runtime_shift,
                .next_accumulator_exponent = next_exponent + (min_safe_shift - ideal_runtime_shift),
            };

        // aligned product now leaves headroom for the sum
        assert((accumulator_bit_count == 0
                   || accumulator_bit_count + coordinate_magnitude_bits - plan.packed_runtime_shift <= max_safe_bits)
            && "shift_planner_t: planned shift fails aligned_product invariant");

        return plan;
    }
};

} // namespace crv::spline
