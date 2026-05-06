// SPx-License-Identifier: MIT

/// \file
/// \brief fixed-point cubic spline segment
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/math/shifter.hpp>
#include <crv/math/spline/polynomial.hpp>
#include <climits>
#include <new>

namespace crv::spline {

/// fixed-point cubic spline segment packed into half a cache line
template <is_fixed t_x_t, is_fixed t_y_t, is_fixed t_coeff_t, is_fixed t_normalized_t, auto polynomial_evaluator,
    auto shifter = shifter_t<rounding_modes::shr::nearest_up>{}>
    requires(is_signed_v<typename t_coeff_t::value_t>)
class alignas(32) segment_t
{
public:
    using x_t = t_x_t;
    using y_t = t_y_t;
    using coeff_t = t_coeff_t;
    using normalized_t = t_normalized_t;

    static_assert(sizeof(coeff_t) > 1); // must have room to pack log2_width

    static constexpr auto coeff_count = 4;
    using coeffs_t = cubic_polynomial_t<coeff_t>;

    constexpr segment_t(coeffs_t coeffs, int8_t log2_width) noexcept : coeffs_{coeffs}
    {
        // this type goes over the ioctl boundary, so it must be trivially copyable
        static_assert(std::is_trivially_copyable_v<segment_t>);

        // this type should fit into no more than half a cache line.
        static_assert(sizeof(segment_t) == 32); // nominal for x64
        static_assert(sizeof(segment_t) * 2 <= std::hardware_constructive_interference_size); // future architectures

        // make sure the top 8 bits of coeff[0] are clear so we can shift it and pack log2_width in the bottom bits
        assert(coeffs_[0] == ((coeffs_[0] << 8) >> 8) && "segment_t: top 8 bits of first coefficient must be clear");

        // pack log2_width into bottom 8 bits of coeff[0]
        coeffs_[0] <<= 8;
        coeffs_[0].value |= (static_cast<uint64_t>(log2_width) & 0xFF);
    }

    /// \pre 0 <= x
    /// \pre x < width()
    constexpr auto x_to_t(x_t x) const noexcept -> normalized_t
    {
        assert(0 <= x);
        assert(x < width());
        return rescale<normalized_t>(x);
    }

    constexpr auto primal(normalized_t t) const noexcept -> y_t
    {
        return y_t::convert(polynomial_evaluator(t, coeffs()));
    }

    constexpr auto tangent(normalized_t t) const noexcept -> y_t
    {
        return y_t::convert(
            polynomial_evaluator(t, quadratic_polynomial_t{3 * unpack_coeff0(), 2 * coeffs_[1], coeffs_[2]}));
    }

    /// extends tangent at t=1 beyond end of segment
    ///
    /// x_extended is the x value relative to the end of the domain, which is relative to the end of the final segment,
    /// which here means relative to t=1: x_extended=0 -> t=1
    ///
    /// \param x_extended x value relative to end of segment
    /// \pre 0 <= x
    [[nodiscard]] constexpr auto extend_final_tangent(x_t x_extended) const noexcept -> y_t
    {
        auto const c0 = unpack_coeff0();
        auto const c1 = coeffs_[1];
        auto const c2 = coeffs_[2];
        auto const c3 = coeffs_[3];

        // segment evaluated at t=1; 1^n = 1, so result is same as sum of coefficients
        auto const p1 = c0 + c1 + c2 + c3;

        // derivative evaluated at t=1: 3*c0*t^2 + 2*c1*t + c2|t=1 -> 3*c0 + 2*c1 + c2
        auto const m1 = 3 * c0 + 2 * c1 + c2;

        auto const wide_product = multiply(m1, x_extended);
        auto const tangent_term = rescale<coeff_t>(wide_product);
        return y_t::convert(p1 + tangent_term);
    }

    /// cubic polynomial coefficients
    constexpr auto coeffs() const noexcept -> coeffs_t { return {unpack_coeff0(), coeffs_[1], coeffs_[2], coeffs_[3]}; }

    /// width of segment as base-2 exponent
    ///
    /// During subdivision, segments are only ever bisected down from a power-of-2 domain, so each segment width has
    /// a single bit set. We can describe that more compactly using this shift from 1.
    constexpr auto log2_width() const noexcept -> int8_t { return static_cast<int8_t>(coeffs_[0].value & 0xFF); }

    /// width of segment in input format
    constexpr auto width() const noexcept -> x_t
    {
        // this doesn't use the shifter because the width is exact.
        return x_t::literal(1ll << (x_t::frac_bits + log2_width()));
    }

    /// validates invariants
    ///
    /// This type goes over the ioctl boundary, so its invariants must be validated before the driver can accept it.
    constexpr auto is_valid() const noexcept -> bool
    {
        constexpr auto bit_width = static_cast<int8_t>(sizeof(typename x_t::value_t) * CHAR_BIT);
        constexpr auto min_log2_width = static_cast<int8_t>(-x_t::frac_bits);
        constexpr auto max_log2_width = static_cast<int8_t>(bit_width - x_t::frac_bits - 2);

        auto const actual_log2_width = log2_width();
        return min_log2_width <= actual_log2_width && actual_log2_width <= max_log2_width;
    }

private:
    constexpr auto unpack_coeff0() const noexcept -> coeff_t { return coeffs_[0] >> 8; }

    template <is_fixed dst_t, is_fixed src_t> constexpr auto rescale(src_t src) const noexcept -> dst_t
    {
        auto const shift_count = dst_t::frac_bits - src_t::frac_bits - log2_width();
        return dst_t::literal(shifter.template shift<typename dst_t::value_t>(src.value, shift_count));
    }

    coeffs_t coeffs_;
};

} // namespace crv::spline
