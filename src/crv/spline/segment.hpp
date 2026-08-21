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
template <typename traits_t, is_fixed t_x_t, is_fixed t_y_t,
    auto shifter = shifter_t<rounding_modes::shr::fast::nearest_up>{},
    auto division_rounding_mode = rounding_modes::div::fast::nearest_away>
struct segment_evaluator_t
{
    using x_t = t_x_t;
    using y_t = t_y_t;

    using unpacked_segment_t = traits_t::unpacked_segment_t;
    using mantissa_t = traits_t::mantissa_t;

    using narrow_t = make_signed_t<mantissa_t>;
    using wide_t = widened_t<narrow_t>;
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

private:
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
        auto const aligned_product = shifter.template shr<narrow_t>(wide_product, relative_shift);
        return add_wrap(aligned_product, coeff);
    }

    constexpr auto align_to_y(mantissa_t accumulator, int_t shift) const noexcept -> y_t
    {
        return y_t::literal(saturate_cast<typename y_t::value_t>(shifter.shift(widen(accumulator), -shift)));
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

private:
    [[no_unique_address]] segment_unpacker_t unpack_segment;
    [[no_unique_address]] segment_evaluator_t evaluate_segment;
    packed_segment_t packed_segment_;
};

} // namespace crv::spline
