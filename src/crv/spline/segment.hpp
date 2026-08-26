// SPDX-License-Identifier: MIT

/// \file
/// \brief dynamically-packed fixed-point induced-gain spline segment
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/math/saturate_cast.hpp>
#include <climits>
#include <type_traits>

namespace crv::spline {

//
// traits
//

constexpr auto dynamic_fields_per_segment = 3;

template <typename t_unpacked_field_t, is_fixed t_y_t> struct unpacked_segment_t
{
    using unpacked_field_t = t_unpacked_field_t;
    using y_t = t_y_t;

    unpacked_field_t d;
    unpacked_field_t c;
    unpacked_field_t b;
    y_t g0;

    constexpr auto operator==(unpacked_segment_t const&) const noexcept -> bool = default;
};

template <typename t_packed_field_t, is_fixed t_y_t> struct packed_segment_t
{
    using packed_field_t = t_packed_field_t;
    using y_t = t_y_t;

    packed_field_t d;
    packed_field_t c;
    packed_field_t b;
    y_t g0;

    constexpr auto operator==(packed_segment_t const&) const noexcept -> bool = default;
};

template <typename t_unpacked_field_t, is_fixed t_y_t> struct traits_t
{
    using unpacked_field_t = t_unpacked_field_t;
    using mantissa_t = unpacked_field_t::mantissa_t;
    using y_t = t_y_t;

    using packed_field_t = make_unsigned_t<mantissa_t>; // [signed mantissa | unsigned shift]

    using packed_segment_t = crv::spline::packed_segment_t<packed_field_t, y_t>;
    using unpacked_segment_t = crv::spline::unpacked_segment_t<unpacked_field_t, y_t>;
};

//
// layouts
//

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

    constexpr auto operator==(field_layout_t const&) const noexcept -> bool = default;
};

template <typename field_layout_t> struct segment_layout_t
{
    field_layout_t intermediate;
    field_layout_t final;

    constexpr auto operator==(segment_layout_t const&) const noexcept -> bool = default;
};

//
// unpacking
//

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

    [[no_unique_address]] field_unpacker_t unpack_field;

    constexpr auto operator()(packed_segment_t const& packed_segment, int_t field_index) const noexcept
        -> unpacked_field_t
    {
        assert(0 <= field_index && field_index < dynamic_fields_per_segment);
        auto const layout
            = (field_index == dynamic_fields_per_segment - 1) ? segment_layout.final : segment_layout.intermediate;
        auto const packed_field = field_index == 0 ? packed_segment.d
            : field_index == 1                     ? packed_segment.c
                                                   : packed_segment.b;
        return unpack_field(packed_field, layout);
    }

    constexpr auto operator()(packed_segment_t const& packed_segment) const noexcept -> unpacked_segment_t
    {
        return unpacked_segment_t{
            .d = unpack_field(packed_segment.d, segment_layout.intermediate),
            .c = unpack_field(packed_segment.c, segment_layout.intermediate),
            .b = unpack_field(packed_segment.b, segment_layout.final),
            .g0 = packed_segment.g0,
        };
    }
};

//
// evaluation
//

/// evaluates gain from one local transfer cubic
///
/// Construction interpolates transfer, but runtime needs gain. Dividing a quantized T(x) by x would amplify error near
/// zero, so each transfer cubic is rewritten as
///
///     T(u) = a + u*S(u),  S(u) = b + c*u + d*u^2,  u = x - x0.
///
/// For x0 > 0, G(x) = S(u) + (x0/x) * (g0 - S(u)), where g0 = a/x0. At x0 == 0, a == 0 and G(x) = S(u), so no
/// division is needed. This is the same transfer cubic in gain form.
template <typename traits_t, is_fixed t_x_t, is_fixed t_y_t, auto rounding_mode = rounding_modes::shr::fast::nearest_up,
    auto division_rounding_mode = rounding_modes::div::fast::nearest_away>
struct segment_evaluator_t
{
    using x_t = t_x_t;
    using y_t = t_y_t;

    using unpacked_segment_t = traits_t::unpacked_segment_t;
    using mantissa_t = traits_t::mantissa_t;

    using narrow_t = make_signed_t<mantissa_t>;
    using wide_t = widened_t<narrow_t>;
    using x_value_t = typename x_t::value_t;
    using y_value_t = typename y_t::value_t;
    using correction_product_t = fixed::product_t<y_t, x_t>;
    using correction_product_value_t = correction_product_t::value_t;

    // correction stays within delta
    //
    // For a located nonzero segment, 0 < x0/x <= 1. delta is already y_t, so the divider cannot grow its magnitude
    // and needs no output saturation.
    using correction_divider_t
        = division::divider_t<typename y_t::value_t, correction_product_value_t, typename x_t::value_t, 0, false>;

    static constexpr auto max_shift = static_cast<int_t>(sizeof(wide_t) * CHAR_BIT) - 1;
    static constexpr auto correction_divide_shift = x_t::frac_bits - correction_product_t::frac_bits + y_t::frac_bits;
    static_assert(correction_divide_shift == 0);
    static_assert(signed_integral<x_value_t>);
    static_assert(signed_integral<y_value_t>);
    static_assert(sizeof(x_value_t) <= sizeof(narrow_t));
    static_assert(sizeof(y_value_t) <= sizeof(wide_t));

    constexpr auto operator()(unpacked_segment_t const& unpacked_segment, x_t x, x_t x0) const noexcept -> y_t
    {
        assert(x0 >= x_t{0});
        assert(x >= x0);

        auto const u = x_t::literal(subtract_wrap(x.value, x0.value));

        if (x0 == x_t{0}) return evaluate_s(unpacked_segment, u);

        auto const s = evaluate_s(unpacked_segment, u);

        assert(x > x_t{0});
        auto const delta = y_t::literal(subtract_wrap(unpacked_segment.g0.value, s.value));
        auto const product = multiply(delta, x0);
        auto const correction = divide<y_t>(product, x, division_rounding_mode, correction_divider_t{});
        return y_t::literal(add_wrap(s.value, correction.value));
    }

    /// proves evaluator arithmetic safe for every local coordinate in [0, u_max]
    constexpr auto is_safe_through(unpacked_segment_t const& unpacked_segment, x_t u_max, x_t x0) const noexcept -> bool
    {
        if (u_max < x_t{0} || x0 < x_t{0}) return false;
        if (u_max.value > max<typename x_t::value_t>() - x0.value) return false;
        if (!valid_shift(unpacked_segment.d.shift) || !valid_shift(unpacked_segment.c.shift)) return false;
        if (!valid_final_shift(unpacked_segment.b.shift)) return false;

        auto bounds = bounds_t{
            .lower = widen(unpacked_segment.d.mantissa),
            .upper = widen(unpacked_segment.d.mantissa),
        };

        if (!apply_coefficient_bounds(
                bounds, unpacked_segment.c.mantissa, unpacked_segment.d.shift, widen_coordinate(u_max), bounds))
        {
            return false;
        }

        if (!apply_coefficient_bounds(
                bounds, unpacked_segment.b.mantissa, unpacked_segment.c.shift, widen_coordinate(u_max), bounds))
        {
            return false;
        }

        auto s_bounds = bounds_t{};
        if (!align_to_y_bounds(bounds, unpacked_segment.b.shift, s_bounds)) return false;
        if (x0 == x_t{0}) return true;

        // correction delta must be representable before multiplication/division
        auto const g0 = int_cast<wide_t>(unpacked_segment.g0.value);
        auto const delta_lower = g0 - s_bounds.upper;
        auto const delta_upper = g0 - s_bounds.lower;
        auto const y_min = int_cast<wide_t>(min<y_value_t>());
        auto const y_max = int_cast<wide_t>(max<y_value_t>());
        if (delta_lower < y_min || delta_upper > y_max) return false;

        auto const x0_wide = int_cast<wide_t>(x0.value);
        auto const product_lower = delta_lower < 0 ? delta_lower * x0_wide : wide_t{0};
        auto const product_upper = delta_upper > 0 ? delta_upper * x0_wide : wide_t{0};
        auto const product_min = int_cast<wide_t>(min<correction_product_value_t>());
        auto const product_max = int_cast<wide_t>(max<correction_product_value_t>());
        if (product_lower < product_min || product_upper > product_max) return false;

        // fast division adds divisor/2 to the unsigned product magnitude before dividing
        using unsigned_product_t = make_unsigned_t<correction_product_value_t>;
        auto const max_product_magnitude = max(unsigned_magnitude(int_cast<correction_product_value_t>(product_lower)),
            unsigned_magnitude(int_cast<correction_product_value_t>(product_upper)));
        auto const x_max = int_cast<unsigned_product_t>(x0.value + u_max.value);
        auto const divide_bias = x_max >> 1;
        if (max_product_magnitude > max<unsigned_product_t>() - divide_bias) return false;

        // x0/x is in (0, 1], so rounded correction magnitude cannot exceed delta. The final sum therefore stays
        // between s and g0 and is representable once delta itself is representable.
        return true;
    }

private:
    struct bounds_t
    {
        wide_t lower;
        wide_t upper;
    };

    static constexpr auto wide_bits = int_t{sizeof(wide_t) * CHAR_BIT};

    template <signed_integral value_t>
    static constexpr auto unsigned_magnitude(value_t value) noexcept -> make_unsigned_t<value_t>
    {
        using unsigned_t = make_unsigned_t<value_t>;
        auto const bits = static_cast<unsigned_t>(value);
        return value < 0 ? static_cast<unsigned_t>(unsigned_t{} - bits) : bits;
    }

    static constexpr auto valid_shift(int_t shift) noexcept -> bool { return 0 <= shift && shift < wide_bits; }

    static constexpr auto valid_final_shift(int_t shift) noexcept -> bool
    {
        return -max_shift <= shift && shift <= max_shift;
    }

    static constexpr auto rounded_shift(wide_t value, int_t shift, wide_t& result) noexcept -> bool
    {
        if (!valid_shift(shift)) return false;
        if (shift == 0)
        {
            result = value;
            return true;
        }

        using unsigned_wide_t = make_unsigned_t<wide_t>;
        auto const half = static_cast<wide_t>((unsigned_wide_t{1} << shift) >> 1);
        if (value > max<wide_t>() - half) return false;

        result = shifter_t<rounding_mode>{}.shr(value, shift);
        return true;
    }

    static constexpr auto widen_coordinate(x_t x) noexcept -> wide_t { return int_cast<wide_t>(x.value); }

    static constexpr auto apply_coefficient_bounds(
        bounds_t accumulator, mantissa_t coefficient, int_t shift, wide_t u_max, bounds_t& result) noexcept -> bool
    {
        if (!valid_shift(shift) || u_max < 0) return false;

        auto const product_lower = accumulator.lower < 0 ? accumulator.lower * u_max : wide_t{0};
        auto const product_upper = accumulator.upper > 0 ? accumulator.upper * u_max : wide_t{0};

        auto aligned_lower = wide_t{};
        auto aligned_upper = wide_t{};
        if (!rounded_shift(product_lower, shift, aligned_lower)) return false;
        if (!rounded_shift(product_upper, shift, aligned_upper)) return false;

        auto const narrow_min = widen(min<narrow_t>());
        auto const narrow_max = widen(max<narrow_t>());
        if (aligned_lower < narrow_min || aligned_upper > narrow_max) return false;

        auto const coefficient_wide = widen(coefficient);
        auto const next_lower = aligned_lower + coefficient_wide;
        auto const next_upper = aligned_upper + coefficient_wide;
        if (next_lower < narrow_min || next_upper > narrow_max) return false;

        result = {.lower = next_lower, .upper = next_upper};
        return true;
    }

    static constexpr auto align_to_y_bounds(bounds_t accumulator, int_t shift, bounds_t& result) noexcept -> bool
    {
        if (!valid_final_shift(shift)) return false;

        auto aligned_lower = wide_t{};
        auto aligned_upper = wide_t{};
        if (shift >= 0)
        {
            if (!rounded_shift(accumulator.lower, shift, aligned_lower)) return false;
            if (!rounded_shift(accumulator.upper, shift, aligned_upper)) return false;
        }
        else
        {
            auto const left_shift = -shift;
            if (left_shift >= wide_bits) return false;

            if (accumulator.lower < (min<wide_t>() >> left_shift)) return false;
            if (accumulator.upper > (max<wide_t>() >> left_shift)) return false;

            aligned_lower = accumulator.lower << left_shift;
            aligned_upper = accumulator.upper << left_shift;
        }

        auto const y_min = int_cast<wide_t>(min<y_value_t>());
        auto const y_max = int_cast<wide_t>(max<y_value_t>());
        result = {
            .lower = max(aligned_lower, y_min),
            .upper = min(aligned_upper, y_max),
        };
        return true;
    }

    constexpr auto evaluate_s(unpacked_segment_t const& unpacked_segment, x_t u) const noexcept -> y_t
    {
        auto accumulator = unpacked_segment.d.mantissa;
        accumulator = apply_coefficient(unpacked_segment.c.mantissa, unpacked_segment.d.shift, u, accumulator);
        accumulator = apply_coefficient(unpacked_segment.b.mantissa, unpacked_segment.c.shift, u, accumulator);
        return align_to_y(accumulator, unpacked_segment.b.shift);
    }

    constexpr auto apply_coefficient(
        mantissa_t coeff, int_t relative_shift, x_t x, mantissa_t accumulator) const noexcept -> mantissa_t
    {
        auto const wide_product = widen(accumulator) * x.value;
        auto const aligned_product = shifter_t<rounding_mode>{}.template shr<narrow_t>(wide_product, relative_shift);
        return add_wrap(aligned_product, coeff);
    }

    constexpr auto align_to_y(mantissa_t accumulator, int_t shift) const noexcept -> y_t
    {
        return y_t::literal(
            saturate_cast<typename y_t::value_t>(shifter_t<rounding_mode>{}.shift(widen(accumulator), -shift)));
    }
};

//
// orchestration
//

/// dynamically packed induced-gain segment occupying half a cache line
///
/// The packed fields are S(u) Horner coefficients. g0 stays as an ordinary y_t value with no dynamic-shift metadata.
template <typename traits_t, is_fixed t_x_t, typename t_segment_unpacker_t, typename t_segment_evaluator_t>
class alignas(32) segment_t
{
public:
    using x_t = t_x_t;
    using segment_unpacker_t = t_segment_unpacker_t;
    using segment_evaluator_t = t_segment_evaluator_t;
    using packed_segment_t = traits_t::packed_segment_t;
    using y_t = segment_evaluator_t::y_t;

    static_assert(segment_unpacker_t::segment_layout.intermediate.max_shift() <= segment_evaluator_t::max_shift);
    static_assert(segment_unpacker_t::segment_layout.final.max_shift() <= segment_evaluator_t::max_shift);
    static_assert(-segment_unpacker_t::segment_layout.final.min_shift() <= segment_evaluator_t::max_shift);

    constexpr segment_t() noexcept : packed_segment_{} {}

    explicit constexpr segment_t(packed_segment_t packed_segment) noexcept : packed_segment_{packed_segment}
    {
        static_assert(std::is_trivially_copyable_v<segment_t>);
        static_assert(alignof(segment_t) >= 32);
    }

    constexpr auto operator()(x_t x, x_t x0) const noexcept -> y_t
    {
        return evaluate_segment(unpack_segment(packed_segment_), x, x0);
    }

    /// proves evaluator arithmetic safe for every local coordinate in [0, u_max]
    constexpr auto is_safe_through(x_t u_max, x_t x0) const noexcept -> bool
    {
        return evaluate_segment.is_safe_through(unpack_segment(packed_segment_), u_max, x0);
    }

private:
    [[no_unique_address]] segment_unpacker_t unpack_segment;
    [[no_unique_address]] segment_evaluator_t evaluate_segment;
    packed_segment_t packed_segment_;
};

} // namespace crv::spline
