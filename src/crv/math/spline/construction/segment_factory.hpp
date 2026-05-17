// SPDX-License-Identifier: MIT

/// \file
/// \brief packing and construction of dynamic segment
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/float_extraction.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/polynomial.hpp>
#include <crv/math/shifter.hpp>
#include <crv/math/spline/construction/function_sampler.hpp>
#include <crv/math/spline/segment.hpp>
#include <algorithm>
#include <climits>

namespace crv::spline {

// --------------------------------------------------------------------------------------------------------------------
// packing
// --------------------------------------------------------------------------------------------------------------------

/// tightly packs individual fields into specific bit ranges of packed_field_t
template <typename t_packed_field_t> struct field_packer_t
{
    using packed_field_t = t_packed_field_t;

    template <typename unpacked_field_t, typename field_layout_t>
    constexpr auto operator()(unpacked_field_t unpacked_field, field_layout_t layout) const noexcept -> packed_field_t
    {
        auto const packed_mantissa = static_cast<packed_field_t>(unpacked_field.mantissa) << layout.shift_width;
        auto const packed_shift = unpacked_field.shift & layout.shift_mask();

        auto const packed_field = packed_field_t{packed_mantissa | packed_shift};
        assert(field_unpacker_t<unpacked_field_t>{}(packed_field, layout) == unpacked_field);

        return packed_field;
    }
};

/// solves relative shifts between dynamic polynomial coefficients
///
/// Coefficients are packed with the relative shifts necessary to align radices between terms rather than with absolute
/// exponents. This type calculates both the relative shift between terms to apply during evaluation, but also
/// calculates the shift to first apply to the terms themselves during construction to prevent overflow.
template <typename shift_t> struct relative_shift_solver_t
{
    struct solved_shift_t
    {
        shift_t accumulator_shift;
        shift_t coeff_shift;
        int_t next_exponent;
    };

    constexpr auto operator()(int_t accumulator_exponent, int_t next_exponent, shift_t t_to_x_shift) const noexcept
        -> solved_shift_t
    {
        auto const relative_shift = next_exponent - accumulator_exponent;

        return {.accumulator_shift = t_to_x_shift + std::max<int_t>(0, relative_shift),
            .coeff_shift = std::max<int_t>(0, -relative_shift),
            .next_exponent = std::max(accumulator_exponent, next_exponent)};
    }
};

/// pre-shifts coefficients so neither they, nor the relative shifts between them, overflow
template <typename unpacked_field_t, auto shifter = shifter_t<>{}> struct coeff_preshifter_t
{
    using mantissa_t = unpacked_field_t::mantissa_t;
    using shift_t = unpacked_field_t::shift_t;

    struct fitted_field_t
    {
        unpacked_field_t field;
        mantissa_t next_accumulator_mantissa;
    };

    static constexpr auto accumulator_width = int_t{sizeof(mantissa_t) * CHAR_BIT} - is_signed_v<mantissa_t>;

    constexpr auto operator()(mantissa_t accumulator_mantissa, mantissa_t next_raw_mantissa, int_t accumulator_shift,
        int_t coeff_shift) const noexcept -> fitted_field_t
    {
        // flush accumulator if it scales out of bounds
        auto accumulator = accumulator_shift >= accumulator_width
            ? unpacked_field_t{}
            : unpacked_field_t{.mantissa = accumulator_mantissa, .shift = accumulator_shift};

        // flush next term if it scales out of bounds
        auto const final_next_mantissa
            = (coeff_shift >= accumulator_width) ? 0 : shifter.shr(next_raw_mantissa, coeff_shift);

        return fitted_field_t{.field = accumulator, .next_accumulator_mantissa = final_next_mantissa};
    }
};

/// aligns radix of the final evaluation step to match the precision of the output type
template <typename unpacked_field_t, typename shift_t, typename t_scaled_int_t, auto align_exponent>
struct radix_aligner_t
{
    using scaled_int_t = t_scaled_int_t;

    constexpr auto operator()(scaled_int_t const& accumulator, int_t radix) const noexcept -> unpacked_field_t
    {
        auto const aligned_accumulator = align_exponent(scaled_int_t{
            .mantissa = accumulator.mantissa, .exponent = int_cast<shift_t>(accumulator.exponent + radix)});

        return unpacked_field_t{
            .mantissa = aligned_accumulator.mantissa,
            .shift = int_cast<shift_t>(-aligned_accumulator.exponent),
        };
    }
};

/// quantizes a segment cubic to an unpacked segment with relative shifts
template <typename t_unpacked_field_t, typename float_extractor_t, typename relative_shift_solver_t, int_t in_frac_bits,
    int_t log2_min_width>
struct segment_quantizer_t
{
    using unpacked_field_t = t_unpacked_field_t;

    using scalar_t = float_extractor_t::scalar_t;
    using cubic_t = cubic_t<scalar_t>;

    struct quantized_term_t
    {
        using shift_t = unpacked_field_t::shift_t;

        unpacked_field_t accumulator;
        shift_t coeff_shift;
    };

    struct quantized_segment_t
    {
        std::array<quantized_term_t, fields_per_segment - 1> transitions;
        unpacked_field_t final_term;
    };

    static_assert(in_frac_bits + log2_min_width >= 0);

    [[no_unique_address]] float_extractor_t extract_float;
    [[no_unique_address]] relative_shift_solver_t solve_relative_shift;

    constexpr auto operator()(cubic_t const& cubic, int_t log2_width) const noexcept -> quantized_segment_t
    {
        auto quantized_segment = quantized_segment_t{};
        auto const seed_it = std::ranges::find_if(cubic, [](auto const& c) { return c != 0.0; });

        if (seed_it == cubic.end()) return quantized_segment;

        auto const t_to_x_shift = in_frac_bits + log2_width;
        auto accumulator = extract_float(*seed_it);

        auto field_index = static_cast<int_t>(std::distance(cubic.begin(), seed_it));
        for (; field_index < fields_per_segment - 1; ++field_index)
        {
            auto const next_term = extract_float(cubic[field_index + 1]);
            auto const effective_next_exp = (next_term.mantissa == 0) ? accumulator.exponent : next_term.exponent;

            auto const shift = solve_relative_shift(accumulator.exponent, effective_next_exp, t_to_x_shift);

            quantized_segment.transitions[field_index] = {
                .accumulator = {
                    .mantissa = accumulator.mantissa,
                    .shift = shift.accumulator_shift,
                },
                .coeff_shift = shift.coeff_shift,
            };

            accumulator.mantissa = next_term.mantissa;
            accumulator.exponent = shift.next_exponent;
        }

        // The final state isolates exactly what the radix aligner needs
        quantized_segment.final_term = {.mantissa = accumulator.mantissa, .shift = accumulator.exponent};

        return quantized_segment;
    }
};

template <typename t_unpacked_segment_t, typename coeff_preshifter_t, typename radix_aligner_t, int_t out_frac_bits>
struct segment_fitter_t
{
    using unpacked_segment_t = t_unpacked_segment_t;
    using scaled_int_t = radix_aligner_t::scaled_int_t;

    [[no_unique_address]] coeff_preshifter_t preshift_coeffs;
    [[no_unique_address]] radix_aligner_t align_radix;

    constexpr auto operator()(auto const& quantized_segment) const noexcept -> unpacked_segment_t
    {
        auto unpacked = unpacked_segment_t{};

        // Find the first valid mantissa to seed the accumulator
        auto current_accumulator_mantissa = quantized_segment.transitions[0].accumulator.mantissa;

        auto const loop_end = fields_per_segment - 2;
        for (int_t i = 0; i < loop_end; ++i)
        {
            auto const& quantized_term = quantized_segment.transitions[i];

            auto const next_raw_mantissa = quantized_segment.transitions[i + 1].accumulator.mantissa;

            auto const fitted = preshift_coeffs(current_accumulator_mantissa, next_raw_mantissa,
                quantized_term.accumulator.shift, quantized_term.coeff_shift);

            unpacked[i] = fitted.field;
            current_accumulator_mantissa = fitted.next_accumulator_mantissa;
        }

        auto const& last_transition = quantized_segment.transitions[loop_end];
        auto const fitted_bridge = preshift_coeffs(current_accumulator_mantissa, quantized_segment.final_term.mantissa,
            last_transition.accumulator.shift, last_transition.coeff_shift);

        unpacked[loop_end] = fitted_bridge.field;
        current_accumulator_mantissa = fitted_bridge.next_accumulator_mantissa;

        auto const final_scaled
            = scaled_int_t{.mantissa = current_accumulator_mantissa, .exponent = quantized_segment.final_term.shift};

        unpacked[fields_per_segment - 1] = align_radix(final_scaled, out_frac_bits);

        return unpacked;
    }
};

/// packs segments tightly according to layout
template <typename t_packed_segment_t, typename unpacked_segment_t, typename field_packer_t,
    segment_layout_t segment_layout>
struct segment_packer_t
{
    using packed_segment_t = t_packed_segment_t;

    [[no_unique_address]] field_packer_t pack_field;

    constexpr auto operator()(unpacked_segment_t const& unpacked_segment) const noexcept -> packed_segment_t
    {
        return packed_segment_t{
            pack_field(unpacked_segment[0], segment_layout.intermediate),
            pack_field(unpacked_segment[1], segment_layout.intermediate),
            pack_field(unpacked_segment[2], segment_layout.intermediate),
            pack_field(unpacked_segment[3], segment_layout.final),
        };
    }
};

/// creates final segment from its cubic and width
template <typename t_segment_t, typename segment_quantizer_t, typename segment_fitter_t, typename segment_packer_t>
struct segment_factory_t
{
    using segment_t = t_segment_t;

    using cubic_t = segment_quantizer_t::cubic_t;

    [[no_unique_address]] segment_quantizer_t quantize_segment;
    [[no_unique_address]] segment_fitter_t fit_segment;
    [[no_unique_address]] segment_packer_t pack_segment;

    constexpr auto operator()(cubic_t const& cubic, int_t log2_width) const noexcept -> segment_t
    {
        auto const quantized_segment = quantize_segment(cubic, log2_width);
        auto const unpacked_segment = fit_segment(quantized_segment);
        auto const packed_segment = pack_segment(unpacked_segment);
        return segment_t{packed_segment};
    }
};

// This condition is what prevents horner's loop intermediates from overflowing. If this assert ever pops, the
// horner's loop generated by this packer will also be broken. The problem is, no specific type has both of these.
//    static_assert(in_frac_bits + log2_min_width >= 0);

} // namespace crv::spline
