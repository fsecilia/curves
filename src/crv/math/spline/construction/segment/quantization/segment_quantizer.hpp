// SPDX-License-Identifier: MIT

/// \file
/// \brief quantizes cubic polynomial to dynamic unpacked segment
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/float_extraction.hpp>
#include <crv/math/polynomial.hpp>
#include <crv/math/spline/segment.hpp>
#include <algorithm>

namespace crv::spline {

// --------------------------------------------------------------------------------------------------------------------
// quantization
// --------------------------------------------------------------------------------------------------------------------

/// quantizes a floating-point cubic into an unpacked segment with relative shifts
template <typename t_unpacked_field_t, typename float_extractor_t, typename shift_planner_t,
    typename mantissa_quantizer_t, typename radix_aligner_t, int_t t_max_intermediate_shift, int_t in_frac_bits,
    int_t out_frac_bits, int_t log2_min_width>
struct segment_quantizer_t
{
    using unpacked_field_t = t_unpacked_field_t;
    using unpacked_segment_t = std::array<unpacked_field_t, fields_per_segment>;

    using mantissa_t = unpacked_field_t::mantissa_t;
    using scalar_t = float_extractor_t::scalar_t;
    using cubic_t = cubic_t<scalar_t>;
    using scaled_int_t = radix_aligner_t::scaled_int_t;

    static constexpr auto max_intermediate_shift = t_max_intermediate_shift;

    // prevent left shifts during evaluation
    //
    // This is the base amount shifted right after every iteration of the evaluation loop. If this is negative, the loop
    // diverges and amplifies after each step instead of contracting.
    static_assert(in_frac_bits + log2_min_width >= 0);

    [[no_unique_address]] float_extractor_t extract_float;
    [[no_unique_address]] shift_planner_t plan_shift;
    [[no_unique_address]] mantissa_quantizer_t quantize_mantissa;
    [[no_unique_address]] radix_aligner_t align_radix;

    constexpr auto operator()(cubic_t const& cubic, int_t log2_width) const noexcept -> unpacked_segment_t
    {
        assert(log2_width >= log2_min_width);
        auto const t_to_x_shift = in_frac_bits + log2_width;

        auto unpacked = unpacked_segment_t{};

        // extract initial accumulator
        auto next_term = extract_float(cubic[0]);
        auto accumulator_mantissa = int_cast<mantissa_t>(next_term.mantissa);
        auto accumulator_exponent = next_term.exponent;

        // proceed in pairs
        for (auto field_index = 0; field_index < fields_per_segment - 1; ++field_index)
        {
            next_term = extract_float(cubic[field_index + 1]);

            // preserve exponent across zero terms
            //
            // A mantissa of 0 has no intrinsic magnitude. By mutually adapting their exponents, we guarantee a relative
            // shift of 0 when either term is exactly 0. This prevents meaningless exponents from causing destructive
            // preshifts or triggering false flushes.
            auto const eval_next_exponent = (next_term.mantissa == 0) ? accumulator_exponent : next_term.exponent;
            auto const eval_accumulator_exponent
                = (accumulator_mantissa == 0) ? eval_next_exponent : accumulator_exponent;

            // plan and apply shifts
            auto const plan = plan_shift(eval_accumulator_exponent, eval_next_exponent, t_to_x_shift);
            if (plan.packed_runtime_shift > max_intermediate_shift)
            {
                // flush earlier terms to zero and restart from here
                //
                // The current term's relative right shift is so large it shifts off all bits accumulated by previous
                // terms. Allowing this zero output but maintaining the large shift would break the relative shift to
                // remaining terms. Instead, equivalently zero all of the terms up to this point and restart from here.

                // flush earlier terms to zero
                std::fill_n(std::begin(unpacked), field_index, {});

                // adopt next_term as the new accumulator baseline
                accumulator_mantissa = int_cast<mantissa_t>(next_term.mantissa);
                accumulator_exponent = eval_next_exponent;
                continue;
            }

            auto const quantized_next = quantize_mantissa(next_term.mantissa, plan.destructive_preshift);
            unpacked[field_index] = {.mantissa = accumulator_mantissa, .shift = plan.packed_runtime_shift};

            // step forward
            accumulator_mantissa = quantized_next;
            accumulator_exponent = plan.next_accumulator_exponent;
        }

        // align final coefficient to the output radix
        unpacked[fields_per_segment - 1] = align_radix(
            scaled_int_t{.mantissa = accumulator_mantissa, .exponent = accumulator_exponent}, out_frac_bits);

        return unpacked;
    }
};

} // namespace crv::spline
