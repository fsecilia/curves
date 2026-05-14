// SPDX-License-Identifier: MIT

/// \file
/// \brief packing and construction of dynamic packed segment
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/shifter.hpp>
#include <crv/math/spline/packed_segment.hpp>
#include <crv/math/spline/polynomial.hpp>
#include <algorithm>
#include <bit>
#include <climits>
#include <concepts>
#include <utility>

namespace crv::spline {

// --------------------------------------------------------------------------------------------------------------------
// packing
// --------------------------------------------------------------------------------------------------------------------

template <typename t_mantissa_t> struct scaled_int_t
{
    using mantissa_t = t_mantissa_t;
    using exponent_t = int8_t;

    mantissa_t mantissa;
    exponent_t exponent;

    constexpr auto operator<=>(scaled_int_t const&) const noexcept -> auto = default;
    constexpr auto operator==(scaled_int_t const&) const noexcept -> bool = default;
};

// extracts integer mantissa and exponent from a float
template <std::floating_point t_scalar_t> struct float_extractor_t
{
    using scalar_t = t_scalar_t;

    static constexpr auto bit_count = int_t{sizeof(scalar_t) * CHAR_BIT};

    using unsigned_t = int_by_bits_t<bit_count, false>;
    using signed_t = int_by_bits_t<bit_count, true>;
    using scaled_int_t = scaled_int_t<signed_t>;

    // ieee constants
    static constexpr auto frac_bit_count = std::numeric_limits<scalar_t>::digits - 1; // -1 for implicit bit
    static constexpr auto frac_mask = (unsigned_t{1} << frac_bit_count) - 1;
    static constexpr auto exp_bit_count = bit_count - 1 - frac_bit_count; // -1 for implicit bit
    static constexpr auto exponent_mask = (unsigned_t{1} << exp_bit_count) - 1;
    static constexpr auto exponent_bias = signed_t{(unsigned_t{1} << (exp_bit_count - 1)) - 1};
    static constexpr auto implicit_bit = unsigned_t{1} << frac_bit_count;

    constexpr auto operator()(scalar_t val) const noexcept -> scaled_int_t
    {
        auto const bits = std::bit_cast<unsigned_t>(val);
        auto const raw_exponent = (bits >> frac_bit_count) & exponent_mask;

        // inf and nan are not supported
        assert(raw_exponent != exponent_mask);

        // ftz; flush denormals to zero
        if (raw_exponent == 0) return {};

        using mantissa_t = scaled_int_t::mantissa_t;
        auto const raw_magnitude = (bits & frac_mask) | implicit_bit;
        auto mantissa = static_cast<mantissa_t>(raw_magnitude);
        assert(static_cast<unsigned_t>(mantissa) == raw_magnitude);
        auto const is_negative = (bits >> (bit_count - 1)) != 0;
        if (is_negative) mantissa = -mantissa;

        using exponent_t = scaled_int_t::exponent_t;
        auto const exponent = int_cast<exponent_t>(
            int_cast<signed_t>(raw_exponent) - exponent_bias - int_cast<signed_t>(frac_bit_count));

        return scaled_int_t{.mantissa = mantissa, .exponent = exponent};
    }
};

template <shift_t t_exponent_min, shift_t t_exponent_max, auto shifter = saturating_shifter_t<>{}>
struct exponent_renormalizer_t
{
    static constexpr shift_t exponent_min = t_exponent_min;
    static constexpr shift_t exponent_max = t_exponent_max;

    template <typename scaled_int_t> constexpr auto operator()(scaled_int_t src) const noexcept -> scaled_int_t
    {
        auto const exponent_clamped = std::clamp(src.exponent, exponent_min, exponent_max);
        auto const left_shift = src.exponent - exponent_clamped;
        return {.mantissa = shifter.shift(src.mantissa, left_shift), .exponent = exponent_clamped};
    }
};

struct field_packer_t
{
    static constexpr auto field_width = int_t{sizeof(packed_field_t) * CHAR_BIT};

    constexpr auto operator()(unpacked_field_t unpacked_field, field_layout_t layout) const noexcept -> packed_field_t
    {
        auto const packed_mantissa = static_cast<packed_field_t>(unpacked_field.mantissa) << layout.shift_width;
        auto const packed_shift = unpacked_field.shift & layout.shift_mask();

        auto const packed_field = packed_field_t{packed_mantissa | packed_shift};
        assert(field_unpacker_t{}(packed_field, layout) == unpacked_field);

        return packed_field;
    }
};

template <typename t_scaled_int_t, is_fixed t_x_t, is_fixed t_y_t, typename t_exponent_renormalizer_t,
    auto shifter = shifter_t<>{}>
struct segment_builder_t
{
    using scaled_int_t = t_scaled_int_t;
    using x_t = t_x_t;
    using y_t = t_y_t;
    using exponent_renormalizer_t = t_exponent_renormalizer_t;

    static constexpr auto in_frac_bits = t_x_t::frac_bits;
    static constexpr auto out_frac_bits = t_y_t::frac_bits;

    static constexpr auto accumulator_width = int_t{sizeof(mantissa_t) * CHAR_BIT};

    int_t t_to_dx_shift;
    int_t acc_exp;
    mantissa_t prev_mantissa;
    exponent_renormalizer_t renormalize_exponent;

    constexpr auto push(scaled_int_t const& next) noexcept -> unpacked_field_t
    {
        assert(t_to_dx_shift >= 0);

        // zero mantissa can't dominate the scale; treat it as matching the accumulator.
        auto const next_exp = (next.mantissa == 0) ? acc_exp : next.exponent;
        auto const exp_gap = next_exp - acc_exp;

        // t_to_dx_shift absorbs dx bit-growth; exp_gap lifts the accumulator when the next coeff is larger.
        auto const acc_shift = t_to_dx_shift + std::max<int_t>(0, exp_gap);
        auto const coeff_shift = std::max<int_t>(0, -exp_gap);

        // flush out-of-scale terms; shift >= 64 means the value is below the dominant term's ULP.
        auto const adjusted_mantissa = (coeff_shift >= accumulator_width) ? 0 : shifter.shr(next.mantissa, coeff_shift);
        auto const final_acc_mantissa = (acc_shift >= accumulator_width) ? 0 : prev_mantissa;
        auto const final_acc_shift = (acc_shift >= accumulator_width) ? 0 : acc_shift;

        acc_exp = std::max(acc_exp, next_exp);
        prev_mantissa = adjusted_mantissa;

        return unpacked_field_t{
            .mantissa = final_acc_mantissa,
            .shift = int_cast<int8_t>(final_acc_shift),
        };
    }

    constexpr auto finish() const&& noexcept -> unpacked_field_t
    {
        auto const renormalized = renormalize_exponent(
            scaled_int_t{.mantissa = prev_mantissa, .exponent = int_cast<shift_t>(acc_exp + out_frac_bits)});

        return unpacked_field_t{
            .mantissa = renormalized.mantissa,
            .shift = int_cast<shift_t>(-renormalized.exponent),
        };
    }
};

template <typename t_builder_t> struct builder_factory_t
{
    using builder_t = t_builder_t;

    constexpr auto operator()(int_t t_to_dx_shift, int_t acc_exp, mantissa_t prev_mantissa) const noexcept -> builder_t
    {
        return builder_t{
            .t_to_dx_shift = t_to_dx_shift,
            .acc_exp = acc_exp,
            .prev_mantissa = prev_mantissa,
            .renormalize_exponent = {},
        };
    }
};

template <typename t_float_extractor_t, typename t_field_packer_t, typename t_builder_factory_t, int_t log2_min_width>
struct segment_packer_t
{
    using float_extractor_t = t_float_extractor_t;
    using field_packer_t = t_field_packer_t;
    using builder_factory_t = t_builder_factory_t;
    using segment_builder_t = builder_factory_t::builder_t;

    using x_t = segment_builder_t::x_t;
    using y_t = segment_builder_t::y_t;
    using scaled_int_t = float_extractor_t::scaled_int_t;
    using scalar_t = float_extractor_t::scalar_t;
    using polynomial_t = polynomial_t<scalar_t>;

    static constexpr auto in_frac_bits = x_t::frac_bits;
    static constexpr auto out_frac_bits = y_t::frac_bits;

    // This condition is what prevents horner's loop intermediates from overflowing. If this assert ever pops, the
    // horner's loop generated by this packer will also be broken.
    static_assert(in_frac_bits + log2_min_width >= 0);

    [[no_unique_address]] field_packer_t pack_field;
    [[no_unique_address]] float_extractor_t extract_float;
    [[no_unique_address]] builder_factory_t make_builder;

    constexpr auto operator()(polynomial_t const& polynomial, int_t log2_width) const noexcept -> packed_segment_t
    {
        auto packed_segment = packed_segment_t{};

        // skip zero prefix
        auto const seed_it = std::ranges::find_if(polynomial, [](auto const& c) { return c != 0.0; });

        // handle degenerate polynomials
        if (seed_it == polynomial.end()) return packed_segment;

        // seed
        auto const t_to_dx_shift = in_frac_bits + log2_width;
        auto const seed = extract_float(*seed_it);
        auto builder = make_builder(t_to_dx_shift, seed.exponent, seed.mantissa);

        // process intermediate suffix
        auto field_index = static_cast<int_t>(std::distance(polynomial.begin(), seed_it));
        for (; field_index < fields_per_segment - 1; ++field_index)
        {
            auto const scaled_int = extract_float(polynomial[field_index + 1]);
            packed_segment[field_index] = pack_field(builder.push(scaled_int), intermediate_layout);
        }

        // finish final field
        packed_segment[fields_per_segment - 1] = pack_field(std::move(builder).finish(), final_layout);

        return packed_segment;
    }
};

} // namespace crv::spline
