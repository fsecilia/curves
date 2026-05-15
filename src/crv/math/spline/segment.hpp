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
#include <crv/math/saturate_cast.hpp>
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

    constexpr auto min_shift() const noexcept -> int_t
    {
        if (!is_signed) return 0;
        return -max_shift() - 1;
    }

    constexpr auto max_shift() const noexcept -> int_t
    {
        if (!is_signed) return static_cast<int_t>(shift_mask());
        return static_cast<int_t>((packed_field_t{1} << shift_width) >> 1) - 1;
    }
};

struct segment_layout_t
{
    field_layout_t intermediate;
    field_layout_t final;

    constexpr auto max_total_shift() const noexcept -> int_t { return intermediate.max_shift() + final.max_shift(); }
};

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

template <typename t_field_unpacker_t, segment_layout_t t_segment_layout> struct segment_unpacker_t
{
    using field_unpacker_t = t_field_unpacker_t;

    static constexpr auto segment_layout = t_segment_layout;
    static constexpr auto max_total_shift = segment_layout.max_total_shift();

    [[no_unique_address]] field_unpacker_t unpack_field;

    constexpr auto operator()(packed_segment_t const& packed_segment, int_t field_index) const noexcept
        -> unpacked_field_t
    {
        auto const layout
            = (field_index == fields_per_segment - 1) ? segment_layout.final : segment_layout.intermediate;
        return unpack_field(packed_segment[field_index], layout);
    }

    constexpr auto operator()(packed_segment_t const& packed_segment) const noexcept -> unpacked_segment_t
    {
        return unpacked_segment_t{
            unpack_field(packed_segment[0], segment_layout.intermediate),
            unpack_field(packed_segment[1], segment_layout.intermediate),
            unpack_field(packed_segment[2], segment_layout.intermediate),
            unpack_field(packed_segment[3], segment_layout.final),
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

    static constexpr auto max_shift = static_cast<int_t>(sizeof(wide_t) * CHAR_BIT) - 1;

    constexpr auto operator()(unpacked_segment_t const& unpacked_segment, x_t const& x) const noexcept -> y_t
    {
        auto const accumulator = apply_coefficients(unpacked_segment, x);
        return apply_final_coefficient(unpacked_segment, x, accumulator);
    }

private:
    // applies horner's loop using dynamic shifts; does not apply final coefficient
    constexpr auto apply_coefficients(unpacked_segment_t const& unpacked_segment, x_t const& x) const noexcept
        -> mantissa_t
    {
        auto accumulator = unpacked_segment[0].mantissa;

        for (auto field_index = 1; field_index < fields_per_segment - 1; ++field_index)
        {
            accumulator = apply_coefficient(
                unpacked_segment[field_index].mantissa, unpacked_segment[field_index - 1].shift, x, accumulator);
        }

        return accumulator;
    }

    constexpr auto apply_coefficient(
        mantissa_t coeff, shift_t relative_shift, x_t const& x, mantissa_t accumulator) const noexcept -> mantissa_t
    {
        // multiply wide
        auto const wide_product = widen(accumulator) * x.value;

        // align product to coeff
        auto const aligned_product = shifter.template shr<narrow_t>(wide_product, relative_shift);

        // sum aligned terms
        //
        // This can technically overflow, but it would require a malformed segment and it is not exploitable.
        return aligned_product + coeff;
    }

    // applies final xoefficient wide with one shr and one round
    constexpr auto apply_final_coefficient(
        unpacked_segment_t const& unpacked_segment, x_t const& x, mantissa_t accumulator) const noexcept -> y_t
    {
        auto const wide_product = widen(accumulator) * x.value;

        // align coeff to product
        auto const relative_shift_c2_to_c3 = unpacked_segment[2].shift;
        auto const aligned_c3 = widen(unpacked_segment[3].mantissa) << relative_shift_c2_to_c3;

        auto const relative_shift_c3_to_y = unpacked_segment[3].shift;
        auto const wide_accumulator = wide_product + aligned_c3;
        auto const total_left_shift = -(relative_shift_c2_to_c3 + relative_shift_c3_to_y);

        // align to the final output radix
        return y_t::literal(saturate_cast<narrow_t>(shifter.shift(wide_accumulator, total_left_shift)));
    }
};

// --------------------------------------------------------------------------------------------------------------------
// orchestration
// --------------------------------------------------------------------------------------------------------------------

/// dynamic, fixed-point cubic spline segment packed into half a cache line
template <is_fixed t_x_t, typename t_packed_segment_t, typename t_segment_unpacker_t, typename t_segment_evaluator_t>
class alignas(32) segment_t
{
public:
    using x_t = t_x_t;
    using packed_segment_t = t_packed_segment_t;
    using segment_unpacker_t = t_segment_unpacker_t;
    using segment_evaluator_t = t_segment_evaluator_t;

    using y_t = segment_evaluator_t::y_t;

    // enforce evaluator's accumulator is wide enough for layout's max shifts
    static_assert(segment_unpacker_t::max_total_shift <= segment_evaluator_t::max_shift);

    constexpr segment_t() noexcept : packed_segment_{} {}

    explicit constexpr segment_t(packed_segment_t packed_segment) noexcept : packed_segment_{packed_segment}
    {
        // this type goes over the ioctl boundary, so it must be trivially copyable
        static_assert(std::is_trivially_copyable_v<segment_t>);

        // this type must be aligned to at least half a cache line; during prod it must be exactly 32, but during
        // testing, it may be overaligned
        static_assert(alignof(segment_t) >= 32);
    }

    constexpr auto operator()(x_t x) const noexcept -> y_t
    {
        return evaluate_segment(unpack_segment(packed_segment_), x);
    }

private:
    [[no_unique_address]] segment_unpacker_t unpack_segment;
    [[no_unique_address]] segment_evaluator_t evaluate_segment;
    packed_segment_t packed_segment_;
};

} // namespace crv::spline
