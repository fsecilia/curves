// SPDX-License-Identifier: MIT

/// \file
/// \brief determines dynamic shifts during evaluation
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/abs.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <bit>
#include <climits>

namespace crv::spline {

/// determines safe relative shifts to align radices between products and the next coefficient during evaluation
///
/// The evaluator only shifts right. This type determines if a simple right shift is sufficient, how much to shift, and
/// how much to destructively preshift the mantissa when a left shift is required.
struct shift_planner_t
{
    struct plan_t
    {
        int_t packed_runtime_shift; ///< relative shift performed at runtime during evaluation
        int_t destructive_preshift; ///< shift applied to next coefficient before packing
        int_t next_accumulator_exponent; ///< state carried to next planning step

        auto operator==(plan_t const&) const noexcept -> bool = default;
    };

    template <signed_integral mantissa_t>
    constexpr auto operator()(mantissa_t accumulator_mantissa, int_t accumulator_exponent, int_t next_exponent,
        int_t t_to_x_shift) const noexcept -> plan_t
    {
        // determine how many bits the accumulator uses
        using unsigned_t = make_unsigned_t<mantissa_t>;
        auto const abs_accumulator_mantissa = int_cast<unsigned_t>(abs(accumulator_mantissa));
        auto const accumulator_mantissa_bit_count = abs_accumulator_mantissa == 0
            ? 0
            : static_cast<int_t>(sizeof(unsigned_t) * CHAR_BIT - std::countl_zero(abs_accumulator_mantissa));

        // determine how many magnitude bits the accumulator can hold:
        //  - -1 for the sign bit
        //  - -1 for headroom when summing to prevent wrapping
        static constexpr auto max_safe_bits = static_cast<int_t>(sizeof(mantissa_t) * CHAR_BIT) - 2;

        // determine min shift to keep result in range
        //
        // Multiplying by x at runtime adds up to `t_to_x_shift` bits of magnitude. This is the shift floor to keep the
        // result <= max_safe_bits.
        auto const min_safe_shift = max<int_t>(0, accumulator_mantissa_bit_count + t_to_x_shift - max_safe_bits);

        auto const relative_shift = next_exponent - accumulator_exponent;
        auto const ideal_runtime_shift = t_to_x_shift + relative_shift;

        if (ideal_runtime_shift >= min_safe_shift)
        {
            // dynamic shift has room to absorb mantissa's exponent
            return {
                .packed_runtime_shift = ideal_runtime_shift,
                .destructive_preshift = 0,
                .next_accumulator_exponent = next_exponent,
            };
        }
        else
        {
            // exponent is larger than dynamic shift can absorb; destructively right-shift mantissa
            auto const shortfall = min_safe_shift - ideal_runtime_shift;
            return {
                .packed_runtime_shift = min_safe_shift,
                .destructive_preshift = shortfall,
                .next_accumulator_exponent = next_exponent + shortfall,
            };
        }
    }
};

} // namespace crv::spline
