// SPDX-License-Identifier: MIT

/// \file
/// \brief dynamic, fixed-point cubic spline segment
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/integer.hpp>
#include <array>
#include <climits>
#include <numeric>

namespace crv::spline {

// --------------------------------------------------------------------------------------------------------------------
// layouts
// --------------------------------------------------------------------------------------------------------------------

using mantissa_t = int64_t;
using shift_t = int8_t;
using packed_field_t = uint64_t; // [signed mantissa | unsigned shift]

struct field_layout_t
{
    int_t shift_width;
    bool is_signed;

    constexpr auto shift_mask() const noexcept -> packed_field_t { return (packed_field_t{1} << shift_width) - 1; }
};

constexpr auto intermediate_layout = field_layout_t{
    .shift_width = 6,
    .is_signed = false,
};

constexpr auto final_layout = field_layout_t{
    .shift_width = 7,
    .is_signed = true,
};

constexpr auto final_layout_min_shift = -(1 << (final_layout.shift_width - 1));
constexpr auto final_layout_max_shift = (1 << (final_layout.shift_width - 1)) - 1;

// --------------------------------------------------------------------------------------------------------------------
// unpacking
// --------------------------------------------------------------------------------------------------------------------

struct unpacked_field_t
{
    mantissa_t mantissa;
    shift_t shift;

    constexpr auto operator==(unpacked_field_t const&) const noexcept -> bool = default;
};

constexpr auto fields_per_segment = 4;
using packed_segment_t = std::array<packed_field_t, fields_per_segment>;
using unpacked_segment_t = std::array<unpacked_field_t, fields_per_segment>;

struct field_unpacker_t
{
    constexpr auto operator()(packed_field_t packed_field, field_layout_t layout) const noexcept -> unpacked_field_t
    {
        auto const shift_masked = int_cast<std::make_unsigned_t<shift_t>>(packed_field & layout.shift_mask());

        int8_t shift_val = shift_masked;
        if (layout.is_signed)
        {
            auto const bit_padding = sizeof(shift_t) * CHAR_BIT - layout.shift_width;
            shift_val = int_cast<shift_t>(static_cast<shift_t>(shift_masked << bit_padding) >> bit_padding);
        }

        return {
            .mantissa = static_cast<mantissa_t>(packed_field) >> layout.shift_width,
            .shift = shift_val,
        };
    }
};

template <typename t_field_unpacker_t> struct segment_unpacker_t
{
    using field_unpacker_t = t_field_unpacker_t;

    [[no_unique_address]] field_unpacker_t unpack_field;

    constexpr auto operator()(packed_segment_t const& packed_segment, int_t field_index) const noexcept
        -> unpacked_field_t
    {
        auto const layout = (field_index == fields_per_segment - 1) ? final_layout : intermediate_layout;
        return unpack_field(packed_segment[field_index], layout);
    }

    constexpr auto operator()(packed_segment_t const& packed_segment) const noexcept -> unpacked_segment_t
    {
        return unpacked_segment_t{
            unpack_field(packed_segment[0], intermediate_layout),
            unpack_field(packed_segment[1], intermediate_layout),
            unpack_field(packed_segment[2], intermediate_layout),
            unpack_field(packed_segment[3], final_layout),
        };
    }
};

// --------------------------------------------------------------------------------------------------------------------
// evaluation
// --------------------------------------------------------------------------------------------------------------------

template <is_fixed t_x_t, is_fixed t_y_t, auto shifter = shifter_t<rounding_modes::shr::fast::nearest_up>{}>
struct segment_evaluator_t
{
    using x_t = t_x_t;
    using y_t = t_y_t;

    using narrow_t = make_signed_t<typename y_t::value_t>;
    using wide_t = widened_t<narrow_t>;

    static constexpr auto max_intermediate_shift = intermediate_layout.shift_mask();
    static constexpr auto max_final_shift = final_layout_max_shift;
    static constexpr auto max_total_shift = max_intermediate_shift + max_final_shift;
    static_assert(max_total_shift < static_cast<int_t>(sizeof(wide_t) * CHAR_BIT));

    constexpr auto operator()(unpacked_segment_t const& unpacked_segment, x_t const& dx) const noexcept -> y_t
    {
        auto accumulator = unpacked_segment[0].mantissa;
        for (auto field_index = 1; field_index < fields_per_segment - 1; ++field_index)
        {
            auto const wide_product = widen(accumulator) * dx.value;

            // align product to coeff
            auto const relative_shift = unpacked_segment[field_index - 1].shift;
            auto const aligned_product = shifter.template shr<narrow_t>(wide_product, relative_shift);
            auto const coeff = unpacked_segment[field_index].mantissa;

            // this can technically overflow, but it would require a malformed segment and it is not exploitable
            accumulator = aligned_product + coeff;
        }

        // unroll final loop iteration and do it wide with one shr and one round

        auto const wide_product = widen(accumulator) * dx.value;

        // align coeff to product
        auto const relative_shift_c2_to_c3 = unpacked_segment[2].shift;
        auto const aligned_c3 = widen(unpacked_segment[3].mantissa) << relative_shift_c2_to_c3;

        auto const relative_shift_c3_to_y = unpacked_segment[3].shift;
        auto const wide_accumulator = wide_product + aligned_c3;
        auto const total_left_shift = -(relative_shift_c2_to_c3 + relative_shift_c3_to_y);

        // align to the final output radix
        accumulator = std::saturating_cast<narrow_t>(shifter.shift(wide_accumulator, total_left_shift));
        return y_t::literal(accumulator);
    }
};

} // namespace crv::spline
