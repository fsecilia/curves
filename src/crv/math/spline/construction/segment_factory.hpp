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
// quantization
// --------------------------------------------------------------------------------------------------------------------

/// solves relative shifts between dynamic polynomial coefficients
///
/// Coefficients are packed with the relative shifts necessary to align radices between terms rather than with absolute
/// exponents. This type calculates both the relative shift between terms to apply during evaluation, but also
/// calculates the shift to first apply to the terms themselves during construction to prevent overflow.
struct relative_shift_solver_t
{
    struct solved_shift_t
    {
        int_t accumulator_shift;
        int_t coeff_shift;
        int_t next_exponent;
    };

    constexpr auto operator()(int_t accumulator_exponent, int_t next_exponent, int_t t_to_x_shift) const noexcept
        -> solved_shift_t
    {
        auto const relative_shift = next_exponent - accumulator_exponent;

        return {
            .accumulator_shift = t_to_x_shift + std::max<int_t>(0, relative_shift),
            .coeff_shift = std::max<int_t>(0, -relative_shift),
            .next_exponent = std::max(accumulator_exponent, next_exponent),
        };
    }
};

/// pre-shifts coefficients so neither they, nor the relative shifts between them, overflow
template <typename unpacked_field_t, auto shifter = shifter_t<>{}> struct coeff_preshifter_t
{
    using mantissa_t = unpacked_field_t::mantissa_t;

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

/// quantizes a floating-point cubic into an unpacked segment with relative shifts
///
/// This type walks the polynomial coefficients, skips leading zero terms, extracts the scaled int from each float,
/// preshifts the next term destructively to prevent overflow, and solves the relative shift to the next term. The final
/// coefficient is aligned to the output radix. Produces an unpacked_segment_t.
template <typename t_unpacked_field_t, typename float_extractor_t, typename relative_shift_solver_t,
    typename coeff_preshifter_t, typename radix_aligner_t, int_t in_frac_bits, int_t out_frac_bits,
    int_t log2_min_width>
struct segment_quantizer_t
{
    using unpacked_field_t = t_unpacked_field_t;
    using unpacked_segment_t = std::array<unpacked_field_t, fields_per_segment>;

    using mantissa_t = unpacked_field_t::mantissa_t;
    using scalar_t = float_extractor_t::scalar_t;
    using cubic_t = cubic_t<scalar_t>;
    using scaled_int_t = radix_aligner_t::scaled_int_t;

    // prevent left shifts during evaluation
    //
    // This is the base amount shifted right after every iteration of the evaluation loop. If this is negative, the loop
    // diverges and amplifies after each step instead of contracting.
    static_assert(in_frac_bits + log2_min_width >= 0);

    [[no_unique_address]] float_extractor_t extract_float;
    [[no_unique_address]] relative_shift_solver_t solve_relative_shift;
    [[no_unique_address]] coeff_preshifter_t preshift_coeffs;
    [[no_unique_address]] radix_aligner_t align_radix;

    constexpr auto operator()(cubic_t const& cubic, int_t log2_width) const noexcept -> unpacked_segment_t
    {
        assert(log2_width >= log2_min_width);

        auto unpacked = unpacked_segment_t{};

        // skip zero prefix
        //
        // A zero coefficient's exponent is 0 by convention (flush-to-zero in the float extractor), which can be
        // arbitrarily far from the next non-zero coefficient's exponent. The relative shift solver would then produce
        // shifts large enough to destroy the non-zero term. Seeding from the first non-zero coefficient avoids this;
        // leading zero fields remain value-initialized and evaluate correctly (0 * x + next = next).
        auto const first_nonzero_coeff
            = std::ranges::find_if(cubic, [](auto const& coeff) noexcept { return coeff != 0.0; });
        if (first_nonzero_coeff == cubic.end()) return unpacked;

        auto const t_to_x_shift = in_frac_bits + log2_width;
        auto const seed = extract_float(*first_nonzero_coeff);

        auto accumulator_mantissa = mantissa_t{seed.mantissa};
        auto accumulator_exponent = seed.exponent;

        auto field_index = static_cast<int_t>(std::distance(cubic.begin(), first_nonzero_coeff));
        for (; field_index < fields_per_segment - 1; ++field_index)
        {
            auto const next_term = extract_float(cubic[field_index + 1]);

            // preserve dynamic range
            //
            // A coefficient of 0 maps to an exponent of 0. When the next coefficient is zero, the relative shift
            // produced would naturally destructively preshift the current coefficient into oblivion. Since the exponent
            // on a zero value is meaningless, maintain the accumulator's exponent instead of resetting it.
            auto const effective_next_exp = (next_term.mantissa == 0) ? accumulator_exponent : next_term.exponent;

            auto const shift = solve_relative_shift(accumulator_exponent, effective_next_exp, t_to_x_shift);
            auto const fitted
                = preshift_coeffs(accumulator_mantissa, next_term.mantissa, shift.accumulator_shift, shift.coeff_shift);

            unpacked[field_index] = fitted.field;
            accumulator_mantissa = fitted.next_accumulator_mantissa;
            accumulator_exponent = shift.next_exponent;
        }

        // align final coefficient to the output radix
        unpacked[fields_per_segment - 1] = align_radix(
            scaled_int_t{.mantissa = accumulator_mantissa, .exponent = accumulator_exponent}, out_frac_bits);

        return unpacked;
    }
};

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

// --------------------------------------------------------------------------------------------------------------------
// orchestration
// --------------------------------------------------------------------------------------------------------------------

/// creates final segment from its cubic and width
template <typename t_segment_t, typename segment_quantizer_t, typename segment_packer_t> struct segment_factory_t
{
    using segment_t = t_segment_t;

    using cubic_t = segment_quantizer_t::cubic_t;

    [[no_unique_address]] segment_quantizer_t quantize_segment;
    [[no_unique_address]] segment_packer_t pack_segment;

    constexpr auto operator()(cubic_t const& cubic, int_t log2_width) const noexcept -> segment_t
    {
        auto const unpacked_segment = quantize_segment(cubic, log2_width);
        auto const packed_segment = pack_segment(unpacked_segment);
        return segment_t{packed_segment};
    }
};

} // namespace crv::spline
