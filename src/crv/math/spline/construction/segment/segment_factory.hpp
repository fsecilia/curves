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
#include <crv/math/spline/construction/segment/function_sampler.hpp>
#include <crv/math/spline/segment.hpp>
#include <algorithm>

namespace crv::spline {

// --------------------------------------------------------------------------------------------------------------------
// quantization
// --------------------------------------------------------------------------------------------------------------------

/// quantizes a floating-point cubic into an unpacked segment with relative shifts
template <typename t_unpacked_field_t, typename float_extractor_t, typename shift_planner_t,
    typename mantissa_quantizer_t, typename radix_aligner_t, int_t in_frac_bits, int_t out_frac_bits,
    int_t log2_min_width>
struct segment_quantizer_t
{
    using unpacked_field_t = t_unpacked_field_t;
    using unpacked_segment_t = std::array<unpacked_field_t, fields_per_segment>;

    using mantissa_t = unpacked_field_t::mantissa_t;
    using scalar_t = float_extractor_t::scalar_t;
    using cubic_t = cubic_t<scalar_t>;
    using scaled_int_t = radix_aligner_t::scaled_int_t;

    static constexpr auto max_container_shift = mantissa_quantizer_t::max_container_shift;

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

        // skip zero prefix
        auto const first_nonzero_coeff
            = std::ranges::find_if(cubic, [](auto const& coeff) noexcept { return coeff != scalar_t{0}; });
        if (first_nonzero_coeff == cubic.end()) return unpacked;

        // extract initial accumulator as a scaled_int
        auto const initial_accumulator = extract_float(*first_nonzero_coeff);
        auto accumulator_mantissa = int_cast<mantissa_t>(initial_accumulator.mantissa);
        auto accumulator_exponent = initial_accumulator.exponent;

        // proceed in pairs
        auto const first_nonzero_field = static_cast<int_t>(std::distance(cubic.begin(), first_nonzero_coeff));
        for (auto field_index = first_nonzero_field; field_index < fields_per_segment - 1; ++field_index)
        {
            auto const next_term = extract_float(cubic[field_index + 1]);

            // preserve exponent across zero terms
            //
            // This prevents a spurious large relative shift to and back from 0 that would obliterate the accumulator.
            auto const effective_next_exponent = (next_term.mantissa == 0) ? accumulator_exponent : next_term.exponent;

            // plan and apply shifts
            auto const plan = plan_shift(accumulator_exponent, effective_next_exponent, t_to_x_shift);
            if (plan.packed_runtime_shift >= max_container_shift) accumulator_mantissa = 0;
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
template <typename t_packed_segment_t, typename unpacked_segment_t, typename field_packer_t, auto segment_layout>
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
