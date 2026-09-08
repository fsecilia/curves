// SPDX-License-Identifier: MIT

/// \file
/// \brief compiles a floating transfer cubic into the fixed induced-gain segment representation
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/bit.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/float_extraction.hpp>
#include <crv/math/polynomial.hpp>
#include <crv/math/shifter.hpp>
#include <crv/spline/segment.hpp>
#include <climits>

namespace crv::spline {

/// applies right-shifts to coefficients, flushing to zero when the shift exceeds container size
template <signed_integral significand_t, auto rounding_mode = rounding_modes::shr::nearest_even>
struct significand_quantizer_t
{
    static constexpr auto max_container_shift = int_t{sizeof(significand_t) * CHAR_BIT} - 1;

    constexpr auto operator()(significand_t significand, int_t preshift) const noexcept -> significand_t
    {
        if (preshift >= max_container_shift) return 0;
        return shifter_t<rounding_mode>{}.shr(significand, preshift);
    }
};

/// aligns radix of the final evaluation step to match the precision of the output type
template <typename unpacked_field_t, typename t_scaled_int_t, auto align_exponent> struct radix_aligner_t
{
    using scaled_int_t = t_scaled_int_t;

    constexpr auto operator()(scaled_int_t const& accumulator, int_t radix) const noexcept -> unpacked_field_t
    {
        auto const aligned_accumulator = align_exponent(
            scaled_int_t{.significand = accumulator.significand, .exponent = accumulator.exponent + radix});

        return unpacked_field_t{
            .significand = aligned_accumulator.significand,
            .shift = -aligned_accumulator.exponent,
        };
    }
};

/// compiles a transfer cubic and exact left derivative into the fixed induced-gain segment
///
/// Dynamic shift planning uses only S(u) = b + c*u + d*u^2. The constant a is stored separately as g0 = a/x0 in y_t,
/// so a large g0 cannot reduce precision in d/c/b. At x0 == 0, transfer requires a == 0 and g0 is unused.
template <typename t_unpacked_segment_t, typename float_extractor_t, typename shift_planner_t,
    typename significand_quantizer_t, typename radix_aligner_t, int_t t_max_intermediate_shift, is_fixed t_x_t>
struct segment_quantizer_t
{
    using unpacked_segment_t = t_unpacked_segment_t;
    using unpacked_field_t = unpacked_segment_t::unpacked_field_t;
    using y_t = unpacked_segment_t::y_t;
    using significand_t = unpacked_field_t::significand_t;
    using scalar_t = float_extractor_t::scalar_t;
    using cubic_t = cubic_t<scalar_t>;
    using scaled_int_t = radix_aligner_t::scaled_int_t;
    using x_t = t_x_t;

    static constexpr auto max_intermediate_shift = t_max_intermediate_shift;
    static constexpr auto coordinate_radix_shift = int_cast<int_t>(x_t::frac_bits);

    [[no_unique_address]] float_extractor_t extract_float;
    [[no_unique_address]] shift_planner_t plan_shift;
    [[no_unique_address]] significand_quantizer_t quantize_significand;
    [[no_unique_address]] radix_aligner_t align_radix;

    constexpr auto operator()(cubic_t const& cubic, scalar_t left_endpoint_derivative, x_t width, x_t x0) const noexcept
        -> unpacked_segment_t
    {
        assert(width > x_t{0});
        assert(x0 >= x_t{0});

        auto const coordinate_magnitude_bits = int_cast<int_t>(bit_width(width.value - 1));
        auto unpacked = unpacked_segment_t{};

        // local quadratic supplies the exact {d, c, b} coefficients used by S's Horner chain
        auto const local_quadratic = polynomial_t{cubic[0], cubic[1], left_endpoint_derivative};
        auto next_term = extract_float(local_quadratic[0]);
        auto accumulator_significand = int_cast<significand_t>(next_term.significand);
        auto accumulator_exponent = next_term.exponent;
        auto runtime_accumulator_bit_count = int_cast<int_t>(bit_width(accumulator_significand));

        for (auto field_index = 0; field_index < dynamic_fields_per_segment - 1; ++field_index)
        {
            next_term = extract_float(local_quadratic[field_index + 1]);

            // zero has no useful exponent; keep relative shift neutral
            auto const eval_next_exponent = (next_term.significand == 0) ? accumulator_exponent : next_term.exponent;
            auto const eval_accumulator_exponent
                = (accumulator_significand == 0) ? eval_next_exponent : accumulator_exponent;

            auto const plan = plan_shift(runtime_accumulator_bit_count, eval_accumulator_exponent, eval_next_exponent,
                coordinate_radix_shift, coordinate_magnitude_bits);
            if (plan.packed_runtime_shift > max_intermediate_shift)
            {
                for (auto preceding_index = 0; preceding_index < field_index; ++preceding_index)
                {
                    dynamic_field(unpacked, preceding_index) = {};
                }

                accumulator_significand = int_cast<significand_t>(next_term.significand);
                accumulator_exponent = eval_next_exponent;
                runtime_accumulator_bit_count = int_cast<int_t>(bit_width(accumulator_significand));
                continue;
            }

            auto const quantized_next = quantize_significand(next_term.significand, plan.destructive_preshift);
            dynamic_field(unpacked, field_index)
                = {.significand = accumulator_significand, .shift = plan.packed_runtime_shift};

            auto const aligned_product_bit_count
                = max<int_t>(0, runtime_accumulator_bit_count + coordinate_magnitude_bits - plan.packed_runtime_shift);
            auto const quantized_next_bit_count = int_cast<int_t>(bit_width(quantized_next));
            auto const carry = (aligned_product_bit_count > 0 && quantized_next_bit_count > 0) ? 1 : 0;
            runtime_accumulator_bit_count = max(aligned_product_bit_count, quantized_next_bit_count) + carry;
            accumulator_significand = quantized_next;
            accumulator_exponent = plan.next_accumulator_exponent;
        }

        unpacked.b = align_radix(
            scaled_int_t{.significand = accumulator_significand, .exponent = accumulator_exponent}, y_t::frac_bits);

        if (x0 == x_t{0})
        {
            assert(cubic[3] == scalar_t{0} && "first transfer segment must satisfy T(0) == 0");
            unpacked.g0 = y_t{0};
        }
        else
        {
            auto const scalar_x0 = from_fixed<scalar_t>(x0);
            unpacked.g0 = to_fixed<y_t>(cubic[3] / scalar_x0);
        }

        return unpacked;
    }

private:
    static constexpr auto dynamic_field(unpacked_segment_t& segment, int_t index) noexcept -> unpacked_field_t&
    {
        assert(0 <= index && index < dynamic_fields_per_segment);
        if (index == 0) return segment.d;
        if (index == 1) return segment.c;
        return segment.b;
    }
};

} // namespace crv::spline
