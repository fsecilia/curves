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
#include <algorithm>
#include <bit>
#include <climits>
#include <concepts>
#include <expected>

namespace crv::spline {

enum class segment_error_reason_t
{
    bad_float,
    coefficient_overflow
};

template <std::floating_point real_t> struct segment_error_t
{
    segment_error_reason_t reason;
    real_t left;
    real_t right;

    constexpr auto operator==(segment_error_t const&) const noexcept -> bool = default;
};

// --------------------------------------------------------------------------------------------------------------------
// packing
// --------------------------------------------------------------------------------------------------------------------

template <auto shifter = shifter_t<>{}> struct saturating_shifter_t
{
    template <typename value_t> constexpr auto shift(value_t value, int_t count) const noexcept -> value_t
    {
        if (count == 0) return value;

        constexpr auto width = int_t{sizeof(mantissa_t) * CHAR_BIT};

        if (count < 0)
        {
            auto const right_shift = -count;
            if (right_shift >= width)
            {
                // right shift saturates to zero when shift exhausts magnitude
                return 0;
            }
            return shifter.template shr<mantissa_t>(value, right_shift);
        }
        else
        {
            if (value == 0) return 0;

            auto const left_shift = count;
            if constexpr (is_signed_v<value_t>)
            {
                if (left_shift >= width) return (value > 0) ? max<mantissa_t>() : min<mantissa_t>();

                auto const max_safe = max<mantissa_t>() >> count;
                if (value > max_safe) return max<mantissa_t>();

                auto const min_safe = min<mantissa_t>() >> count;
                if (value < min_safe) return min<mantissa_t>();
            }
            else
            {
                if (left_shift >= width) return max<mantissa_t>();

                auto const max_safe = max<mantissa_t>() >> count;
                if (value > max_safe) return max<mantissa_t>();
            }

            return shifter.template shl<mantissa_t>(value, count);
        }
    }
};

template <typename real_t> using polynomial_t = std::array<real_t, fields_per_segment>;

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
template <std::floating_point t_real_t> struct float_extractor_t
{
    using real_t = t_real_t;

    static constexpr auto bit_count = int_t{sizeof(real_t) * CHAR_BIT};

    using unsigned_t = int_by_bits_t<bit_count, false>;
    using signed_t = int_by_bits_t<bit_count, true>;
    using scaled_int_t = scaled_int_t<signed_t>;

    // ieee constants
    static constexpr auto frac_bit_count = std::numeric_limits<real_t>::digits - 1; // -1 for implicit bit
    static constexpr auto frac_mask = (unsigned_t{1} << frac_bit_count) - 1;
    static constexpr auto exp_bit_count = bit_count - 1 - frac_bit_count; // -1 for implicit bit
    static constexpr auto exponent_mask = (unsigned_t{1} << exp_bit_count) - 1;
    static constexpr auto exponent_bias = signed_t{(unsigned_t{1} << (exp_bit_count - 1)) - 1};
    static constexpr auto implicit_bit = unsigned_t{1} << frac_bit_count;

    constexpr auto operator()(real_t val) const noexcept -> std::expected<scaled_int_t, segment_error_reason_t>
    {
        auto const bits = std::bit_cast<unsigned_t>(val);
        auto const raw_exponent = (bits >> frac_bit_count) & exponent_mask;

        // inf and nan are not supported
        auto const bad_float = raw_exponent == exponent_mask;
        if (bad_float) return std::unexpected(segment_error_reason_t::bad_float);

        // ftz
        if (raw_exponent == 0) return {};

        using mantissa_t = scaled_int_t::mantissa_t;
        auto const raw_magnitude = (bits & frac_mask) | implicit_bit;
        auto mantissa = static_cast<mantissa_t>(raw_magnitude);
        auto const is_negative = (bits >> (bit_count - 1)) != 0;
        if (is_negative) mantissa = -mantissa;

        using exponent_t = scaled_int_t::exponent_t;
        auto const exponent = int_cast<exponent_t>(
            int_cast<signed_t>(raw_exponent) - exponent_bias - int_cast<signed_t>(frac_bit_count));

        return scaled_int_t{.mantissa = mantissa, .exponent = exponent};
    }
};

struct field_packer_t
{
    static constexpr auto field_width = int_t{sizeof(packed_field_t) * CHAR_BIT};

    constexpr auto operator()(unpacked_field_t unpacked_field, field_layout_t layout) const noexcept
        -> std::expected<packed_field_t, segment_error_reason_t>
    {
        auto const mantissa_bits = field_width - layout.shift_width;

        // 2^(N-1) - 1 for max, -2^(N-1) for min
        auto const max_mantissa = (mantissa_t{1} << (mantissa_bits - 1)) - 1;
        auto const min_mantissa = -(mantissa_t{1} << (mantissa_bits - 1));

        if (unpacked_field.mantissa > max_mantissa || unpacked_field.mantissa < min_mantissa)
        {
            return std::unexpected(segment_error_reason_t::coefficient_overflow);
        }

        auto const packed_mantissa = static_cast<packed_field_t>(unpacked_field.mantissa) << layout.shift_width;
        auto const packed_shift = unpacked_field.shift & layout.shift_mask();
        return packed_field_t{packed_mantissa | packed_shift};
    }
};

template <typename t_scaled_int_t, is_fixed t_x_t, is_fixed t_y_t> struct segment_builder_t
{
    using scaled_int_t = t_scaled_int_t;
    using x_t = t_x_t;
    using y_t = t_y_t;

    static constexpr auto in_frac_bits = t_x_t::frac_bits;
    static constexpr auto out_frac_bits = t_y_t::frac_bits;

    static constexpr auto accumulator_width = int_t{sizeof(mantissa_t) * CHAR_BIT};

    int_t delta;
    int_t acc_exp;
    mantissa_t prev_mantissa;

    constexpr auto push(scaled_int_t const& next) noexcept -> unpacked_field_t
    {
        // zero mantissa can't dominate the scale; treat it as matching the accumulator.
        auto const next_exp = (next.mantissa == 0) ? acc_exp : next.exponent;
        auto const exp_gap = next_exp - acc_exp;

        // delta absorbs dx bit-growth; exp_gap lifts the accumulator when the next coeff is larger.
        auto const acc_shift = delta + std::max<int_t>(0, exp_gap);
        auto const coeff_shift = std::max<int_t>(0, -exp_gap);

        // flush out-of-scale terms; shift >= 64 means the value is below the dominant term's ULP.
        auto const adjusted_mantissa = (coeff_shift >= accumulator_width) ? 0 : (next.mantissa >> coeff_shift);
        auto const final_acc_mantissa = (acc_shift >= accumulator_width) ? 0 : prev_mantissa;
        auto const final_acc_shift = (acc_shift >= accumulator_width) ? 0 : acc_shift;

        acc_exp = std::max(acc_exp, next_exp);
        prev_mantissa = adjusted_mantissa;

        return unpacked_field_t{
            .mantissa = final_acc_mantissa,
            .shift = int_cast<int8_t>(final_acc_shift),
        };
    }

    constexpr auto finish() const noexcept -> unpacked_field_t
    {
        static constexpr auto max_mantissa = max<mantissa_t>();
        static constexpr auto min_mantissa = min<mantissa_t>();

        static constexpr auto min_final_shift = -(1 << (final_layout.shift_width - 1));
        static constexpr auto max_final_shift = (1 << (final_layout.shift_width - 1)) - 1;

        // final term: no successor, so the shift aligns directly to the output radix
        auto const final_shift = -acc_exp - out_frac_bits;
        auto const clamped_shift = std::clamp<int_t>(final_shift, min_final_shift, max_final_shift);
        auto const delta_shift = final_shift - clamped_shift;

        auto compensated_mantissa = prev_mantissa;

        if (delta_shift > 0)
        {
            compensated_mantissa = (delta_shift >= accumulator_width) ? 0 : (prev_mantissa >> delta_shift);
        }
        else if (delta_shift < 0)
        {
            auto const left_shift = -delta_shift;
            if (prev_mantissa == 0) compensated_mantissa = 0;
            else if (left_shift >= accumulator_width)
            {
                compensated_mantissa = (prev_mantissa > 0) ? max_mantissa : min_mantissa;
            }
            else
            {
                auto const max_safe = max_mantissa >> left_shift;
                auto const min_safe = min_mantissa >> left_shift;

                if (prev_mantissa > max_safe) compensated_mantissa = max_mantissa;
                else if (prev_mantissa < min_safe) compensated_mantissa = min_mantissa;
                else compensated_mantissa = prev_mantissa << left_shift;
            }
        }

        return unpacked_field_t{
            .mantissa = compensated_mantissa,
            .shift = int_cast<shift_t>(clamped_shift),
        };
    }
};

template <typename t_builder_t> struct builder_factory_t
{
    using builder_t = t_builder_t;

    constexpr auto operator()(int_t delta, int_t acc_exp, mantissa_t prev_mantissa) const noexcept -> builder_t
    {
        return builder_t{
            .delta = delta,
            .acc_exp = acc_exp,
            .prev_mantissa = prev_mantissa,
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
    using real_t = float_extractor_t::real_t;
    using polynomial_t = polynomial_t<real_t>;

    static constexpr auto in_frac_bits = x_t::frac_bits;
    static constexpr auto out_frac_bits = y_t::frac_bits;

    // This condition is what prevents horner's loop intermediates from overflowing. If this assert ever pops, the
    // horner's loop generated by this packer will also be broken.
    static_assert(in_frac_bits + log2_min_width >= 0);

    [[no_unique_address]] field_packer_t pack_field;
    [[no_unique_address]] float_extractor_t extract_float;
    [[no_unique_address]] builder_factory_t make_builder;

    constexpr auto operator()(polynomial_t const& polynomial, int_t log2_width) const noexcept
        -> std::expected<packed_segment_t, segment_error_reason_t>
    {
        auto const initial = extract_float(polynomial[0]);
        if (!initial) return std::unexpected(initial.error());

        // delta is the bit-growth from multiplying by dx (the segment-local input step)
        // precondition: delta >= 0 (the segment has at least 1 bit of input resolution)
        auto const delta = in_frac_bits + log2_width;
        assert(delta >= 0);

        auto builder = make_builder(delta, initial->exponent, initial->mantissa);

        packed_segment_t packed_segment;

        for (auto field_index = 0; field_index < fields_per_segment - 1; ++field_index)
        {
            auto const extract_float_result = extract_float(polynomial[field_index + 1]);
            if (!extract_float_result) return std::unexpected(extract_float_result.error());
            auto const& next = *extract_float_result;

            auto const unpacked = builder.push(next);
            auto const pack_result = pack_field(unpacked, intermediate_layout);
            if (!pack_result) return std::unexpected(pack_result.error());
            packed_segment[field_index] = *pack_result;
        }

        auto const final_unpacked = builder.finish();
        auto const final_pack = pack_field(final_unpacked, final_layout);
        if (!final_pack) return std::unexpected(final_pack.error());
        packed_segment[fields_per_segment - 1] = *final_pack;

        return packed_segment;
    }
};

} // namespace crv::spline
