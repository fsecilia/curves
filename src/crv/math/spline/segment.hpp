// SPDX-License-Identifier: MIT

/// \file
/// \brief dynamic, fixed-point cubic spline segment
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/saturate_cast.hpp>
#include <array>
#include <climits>

namespace crv::spline {

// --------------------------------------------------------------------------------------------------------------------
// traits
// --------------------------------------------------------------------------------------------------------------------

constexpr auto fields_per_segment = 4;

template <typename t_unpacked_field_t> struct traits_t
{
    using unpacked_field_t = t_unpacked_field_t;
    using mantissa_t = unpacked_field_t::mantissa_t;

    using packed_field_t = make_unsigned_t<mantissa_t>; // [signed mantissa | unsigned shift]

    using packed_segment_t = std::array<packed_field_t, fields_per_segment>;
    using unpacked_segment_t = std::array<unpacked_field_t, fields_per_segment>;
};

// --------------------------------------------------------------------------------------------------------------------
// layouts
// --------------------------------------------------------------------------------------------------------------------

template <typename t_packed_field_t> struct field_layout_t
{
    using packed_field_t = t_packed_field_t;

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

template <typename field_layout_t> struct segment_layout_t
{
    field_layout_t intermediate;
    field_layout_t final;

    constexpr auto max_total_shift() const noexcept -> int_t { return intermediate.max_shift() + final.max_shift(); }
};

// --------------------------------------------------------------------------------------------------------------------
// unpacking
// --------------------------------------------------------------------------------------------------------------------

template <signed_integral t_mantissa_t> struct unpacked_field_t
{
    using mantissa_t = t_mantissa_t;

    mantissa_t mantissa;
    int_t shift;

    constexpr auto operator==(unpacked_field_t const&) const noexcept -> bool = default;
};

template <typename t_unpacked_field_t> struct field_unpacker_t
{
    using unpacked_field_t = t_unpacked_field_t;

    template <typename packed_field_t, typename field_layout_t>
    constexpr auto operator()(packed_field_t packed_field, field_layout_t layout) const noexcept -> unpacked_field_t
    {
        using mantissa_t = unpacked_field_t::mantissa_t;

        auto const shift_masked = packed_field & layout.shift_mask();
        int_t shift;
        if (layout.is_signed)
        {
            // use arithmetic shift to extend sign by msb into container's msb, then back
            auto const bit_padding = sizeof(int_t) * CHAR_BIT - layout.shift_width;
            shift = static_cast<int_t>(shift_masked << bit_padding) >> bit_padding;
        }
        else
        {
            shift = static_cast<int_t>(shift_masked);
        }

        // use arithmetic shift to extend sign
        auto const mantissa = static_cast<mantissa_t>(packed_field) >> layout.shift_width;

        return {
            .mantissa = mantissa,
            .shift = shift,
        };
    }
};

template <typename packed_segment_t, typename t_unpacked_segment_t, typename field_unpacker_t,
    segment_layout_t t_segment_layout>
struct segment_unpacker_t
{
    using unpacked_segment_t = t_unpacked_segment_t;

    using unpacked_field_t = field_unpacker_t::unpacked_field_t;

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

template <typename traits_t, is_fixed t_x_t, is_fixed t_y_t,
    auto shifter = shifter_t<rounding_modes::shr::fast::nearest_up>{}>
struct segment_evaluator_t
{
    using x_t = t_x_t;
    using y_t = t_y_t;

    using unpacked_segment_t = traits_t::unpacked_segment_t;
    using mantissa_t = traits_t::mantissa_t;

    using narrow_t = make_signed_t<mantissa_t>;
    using wide_t = widened_t<narrow_t>;

    static constexpr auto max_shift = static_cast<int_t>(sizeof(wide_t) * CHAR_BIT) - 1;

    constexpr auto operator()(unpacked_segment_t const& unpacked_segment, x_t const& x) const noexcept -> y_t
    {
        auto accumulator = unpacked_segment[0].mantissa;
        accumulator = apply_coefficient(unpacked_segment[1].mantissa, unpacked_segment[0].shift, x, accumulator);
        accumulator = apply_coefficient(unpacked_segment[2].mantissa, unpacked_segment[1].shift, x, accumulator);
        return apply_final_coefficient(unpacked_segment, x, accumulator);
    }

private:
    // prevents UB from signed integer overflow
    //
    // The segments we generate do not overflow by construction, but a malformed segment might. None of the segment
    // evaluation result is used to index into memory, so an overflow wrapping just means a bad mouse curve, not a CVE.
    // This function is used to sum two signed, 2's complement values using unsigned arithmetic. The result is the same,
    // but overflow just wraps; it does not induce UB.
    template <typename value_t> static constexpr auto safe_add(value_t lhs, value_t rhs) noexcept -> value_t
    {
        return add_wrap(lhs, rhs);
    }

    constexpr auto apply_coefficient(
        mantissa_t coeff, int_t relative_shift, x_t const& x, mantissa_t accumulator) const noexcept -> mantissa_t
    {
        // multiply wide
        auto const wide_product = widen(accumulator) * x.value;

        // align product to coeff
        auto const aligned_product = shifter.template shr<narrow_t>(wide_product, relative_shift);

        // sum aligned terms
        return safe_add(aligned_product, coeff);
    }

    // applies final coefficient wide with one shr and one round
    constexpr auto apply_final_coefficient(
        unpacked_segment_t const& unpacked_segment, x_t const& x, mantissa_t accumulator) const noexcept -> y_t
    {
        auto const wide_product = widen(accumulator) * x.value;

        // align coeff to product
        auto const relative_shift_c_to_d = unpacked_segment[2].shift;
        auto const aligned_d = widen(unpacked_segment[3].mantissa) << relative_shift_c_to_d;

        // align to the final output radix
        auto const relative_shift_d_to_y = unpacked_segment[3].shift;
        auto const wide_accumulator = safe_add(wide_product, aligned_d);
        auto const total_left_shift = -(relative_shift_c_to_d + relative_shift_d_to_y);
        return y_t::literal(saturate_cast<typename y_t::value_t>(shifter.shift(wide_accumulator, total_left_shift)));
    }
};

// --------------------------------------------------------------------------------------------------------------------
// orchestration
// --------------------------------------------------------------------------------------------------------------------

/// dynamic, fixed-point cubic spline segment packed into half a cache line
template <typename traits_t, is_fixed t_x_t, typename t_segment_unpacker_t, typename t_segment_evaluator_t>
class alignas(32) segment_t
{
public:
    using x_t = t_x_t;
    using segment_unpacker_t = t_segment_unpacker_t;
    using segment_evaluator_t = t_segment_evaluator_t;

    using packed_segment_t = traits_t::packed_segment_t;

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
