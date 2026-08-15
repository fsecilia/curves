// SPDX-License-Identifier: MIT

/// \file
/// \brief quantizes cubic polynomial to dynamic unpacked segment
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/bit.hpp>
#include <crv/math/float_extraction.hpp>
#include <crv/math/polynomial.hpp>
#include <crv/math/shifter.hpp>
#include <crv/spline/segment.hpp>
#include <algorithm>
#include <climits>

namespace crv::spline {

/// applies right-shifts to coefficients, flushing to zero when the shift exceeds container size
template <signed_integral mantissa_t, auto shifter = shifter_t<>{}> struct mantissa_quantizer_t
{
    static constexpr auto max_container_shift = int_t{sizeof(mantissa_t) * CHAR_BIT} - 1;

    constexpr auto operator()(mantissa_t mantissa, int_t preshift) const noexcept -> mantissa_t
    {
        if (preshift >= max_container_shift) return 0;
        return shifter.shr(mantissa, preshift);
    }
};

/// aligns radix of the final evaluation step to match the precision of the output type
template <typename unpacked_field_t, typename t_scaled_int_t, auto align_exponent> struct radix_aligner_t
{
    using scaled_int_t = t_scaled_int_t;

    constexpr auto operator()(scaled_int_t const& accumulator, int_t radix) const noexcept -> unpacked_field_t
    {
        auto const aligned_accumulator
            = align_exponent(scaled_int_t{.mantissa = accumulator.mantissa, .exponent = accumulator.exponent + radix});

        return unpacked_field_t{
            .mantissa = aligned_accumulator.mantissa,
            .shift = -aligned_accumulator.exponent,
        };
    }
};

/// quantizes a floating-point local-coordinate cubic into an unpacked segment with relative shifts
template <typename t_unpacked_field_t, typename float_extractor_t, typename shift_planner_t,
    typename mantissa_quantizer_t, typename radix_aligner_t, int_t t_max_intermediate_shift, is_fixed t_x_t,
    int_t out_frac_bits>
struct segment_quantizer_t
{
    using unpacked_field_t = t_unpacked_field_t;
    using unpacked_segment_t = std::array<unpacked_field_t, fields_per_segment>;

    using mantissa_t = unpacked_field_t::mantissa_t;
    using scalar_t = float_extractor_t::scalar_t;
    using cubic_t = cubic_t<scalar_t>;
    using scaled_int_t = radix_aligner_t::scaled_int_t;
    using x_t = t_x_t;

    static constexpr auto max_intermediate_shift = t_max_intermediate_shift;
    static constexpr auto coordinate_radix_shift = int_cast<int_t>(x_t::frac_bits);

    [[no_unique_address]] float_extractor_t extract_float;
    [[no_unique_address]] shift_planner_t plan_shift;
    [[no_unique_address]] mantissa_quantizer_t quantize_mantissa;
    [[no_unique_address]] radix_aligner_t align_radix;

    constexpr auto operator()(cubic_t const& cubic, x_t width) const noexcept -> unpacked_segment_t
    {
        assert(width > x_t{0});

        // Multiplication uses the raw fixed-point coordinate. The radix contribution is always x_t::frac_bits. The
        // actual interval width contributes only to the maximum magnitude of that raw coordinate. bit_width(width-1)
        // is ceil(log2(width)) for a positive integer width, so powers of two retain the old exact magnitude bound.
        auto const coordinate_magnitude_bits = int_cast<int_t>(bit_width(width.value - 1));

        auto unpacked = unpacked_segment_t{};

        // extract initial accumulator
        auto next_term = extract_float(cubic[0]);
        auto accumulator_mantissa = int_cast<mantissa_t>(next_term.mantissa);
        auto accumulator_exponent = next_term.exponent;

        // Upper bound on the runtime accumulator entering the next multiplication. The accumulator is a partial sum,
        // so it may be one bit wider than either operand and must be tracked as state.
        auto runtime_accumulator_bit_count = int_cast<int_t>(bit_width(accumulator_mantissa));

        for (auto field_index = 0; field_index < fields_per_segment - 1; ++field_index)
        {
            next_term = extract_float(cubic[field_index + 1]);

            // Zero has no intrinsic exponent. Keep the relative shift neutral when either term is exactly zero.
            auto const eval_next_exponent = (next_term.mantissa == 0) ? accumulator_exponent : next_term.exponent;
            auto const eval_accumulator_exponent
                = (accumulator_mantissa == 0) ? eval_next_exponent : accumulator_exponent;

            auto const plan = plan_shift(runtime_accumulator_bit_count, eval_accumulator_exponent, eval_next_exponent,
                coordinate_radix_shift, coordinate_magnitude_bits);
            if (plan.packed_runtime_shift > max_intermediate_shift)
            {
                // The next coefficient dominates everything accumulated so far. Flush those earlier terms and restart
                // here so the remaining relative shifts stay representable.
                std::fill_n(std::begin(unpacked), field_index, unpacked_field_t{});

                accumulator_mantissa = int_cast<mantissa_t>(next_term.mantissa);
                accumulator_exponent = eval_next_exponent;
                runtime_accumulator_bit_count = bit_width(accumulator_mantissa);
                continue;
            }

            auto const quantized_next = quantize_mantissa(next_term.mantissa, plan.destructive_preshift);
            unpacked[field_index] = {.mantissa = accumulator_mantissa, .shift = plan.packed_runtime_shift};

            auto const aligned_product_bit_count = max<int_t>(
                0, runtime_accumulator_bit_count + coordinate_magnitude_bits - plan.packed_runtime_shift);
            auto const quantized_next_bit_count = int_cast<int_t>(bit_width(quantized_next));
            auto const carry = (aligned_product_bit_count > 0 && quantized_next_bit_count > 0) ? 1 : 0;
            runtime_accumulator_bit_count = max(aligned_product_bit_count, quantized_next_bit_count) + carry;
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
