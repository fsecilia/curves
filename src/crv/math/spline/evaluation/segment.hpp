// SPDX-License-Identifier: MIT

/// \file
/// \brief fixed-point cubic spline segment
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/fma.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/rounding_mode.hpp>
#include <crv/math/shifter.hpp>
#include <climits>
#include <new>

namespace crv::spline {

// fma with common types and fast nearest_up rounding mode
template <is_fixed in_t, is_fixed coeff_t>
using segment_fma_t = fma_t<coeff_t, in_t, coeff_t, coeff_t, shifter_t<rounding_modes::shr::fast::nearest_up_t>,
    overflow_policy_t::saturate>;

/// fixed-point cubic spline segment packed into half a cache line
template <is_fixed t_x_t, is_fixed t_coeff_t, is_fixed t_y_t = t_coeff_t,
    typename fma_t = segment_fma_t<t_x_t, t_coeff_t>,
    typename shifter_t = shifter_t<rounding_modes::shr::fast::nearest_up_t>>
    requires(is_signed_v<typename t_coeff_t::value_t>)
class alignas(32) segment_t
{
public:
    using x_t = t_x_t;
    using y_t = t_y_t;
    using coeff_t = t_coeff_t;

    static_assert(sizeof(coeff_t) > 1); // must have room to pack log2_width

    static constexpr auto coeff_count = 4;
    using coeffs_t = std::array<coeff_t, coeff_count>;

    constexpr segment_t(coeffs_t coeffs, int8_t log2_width, fma_t fma = {}, shifter_t shifter = {}) noexcept
        : fma_{std::move(fma)}, shifter_{std::move(shifter)}, coeffs_{coeffs}
    {
        // this type goes over the ioctl boundary, so it must be trivially copyable
        static_assert(std::is_trivially_copyable_v<segment_t>);

        // this type should fit into no more than half a cache line.
        static_assert(sizeof(segment_t) == 32); // nominal for x64
        static_assert(sizeof(segment_t) * 2 <= std::hardware_constructive_interference_size); // future architectures

        // make sure the top 8 bits of coeff[0] are clear so we can shift it and pack log2_width in the bottom bits
        assert(coeffs[0] == ((coeffs[0] << 8) >> 8) && "segment_t: top 8 bits of first coefficient must be clear");

        // pack log2_width into bottom 8 bits of coeff[0]
        coeffs_[0] <<= 8;
        coeffs_[0].value |= (static_cast<uint64_t>(log2_width) & 0xFF);
    }

    /// \pre 0 <= dx
    /// \pre dx < width()
    [[nodiscard]] constexpr auto evaluate(x_t dx) const noexcept -> y_t
    {
        assert(dx < width());

        auto const t = dx_to_t(dx);
        auto result = unpack_coeff0();
        for (auto coeff = 1; coeff < coeff_count; ++coeff) result = fma_(result, t, coeffs_[coeff]);

        return y_t::convert(result);
    }

    /// extends tangent at t=1 beyond end of segment
    ///
    /// dx_extended is the x value relative to the end of the domain, which is relative to the end of the final segment,
    /// which here means relative to t=1: dx_extended=0 -> t=1
    ///
    /// \param dx_extended x value relative to end of segment
    /// \pre 0 <= dx
    [[nodiscard]] constexpr auto extend_final_tangent(x_t dx_extended) const noexcept -> y_t
    {
        auto const t = dx_to_t(dx_extended);

        auto const c0 = unpack_coeff0();
        auto const c1 = coeffs_[1];
        auto const c2 = coeffs_[2];
        auto const c3 = coeffs_[3];

        // segment evaluated at t=1; 1^n = 1, so result is same as sum of coefficients
        auto const p1 = c0 + c1 + c2 + c3;

        // derivative evaluated at t=1: 3*c0*t^2 + 2*c1*t + c2|t=1 -> 3*c0 + 2*c1 + c2
        auto const m1 = 3 * c0 + 2 * c1 + c2;

        return y_t::convert(p1 + m1 * t);
    }

    /// width of segment in input format
    constexpr auto width() const noexcept -> x_t
    {
        // this doesn't use the shifter because the width is exact.
        return x_t::literal(1ll << (x_t::frac_bits + unpack_log2_width()));
    }

    /// validates invariants
    ///
    /// This type goes over the ioctl boundary, so its invariants must be validated before the driver can accept it.
    constexpr auto is_valid() const noexcept -> bool
    {
        constexpr auto bit_width = static_cast<int8_t>(sizeof(typename x_t::value_t) * CHAR_BIT);
        constexpr auto min_log2_width = static_cast<int8_t>(-x_t::frac_bits);
        constexpr auto max_log2_width = static_cast<int8_t>(bit_width - x_t::frac_bits - 2);

        auto const log2_width = unpack_log2_width();
        return min_log2_width <= log2_width && log2_width <= max_log2_width;

        return true;
    }

private:
    constexpr auto dx_to_t(x_t dx) const noexcept -> x_t
    {
        assert(x_t{0} <= dx);

        auto const log2_width = unpack_log2_width();

        // find t t by dividing dx by width. This shifts in the opposite direction log2 would:
        // x/2^k = x*2^-k = x << -k == x >> k
        return x_t::literal(shifter_.shift(dx.value, -log2_width));
    }

    constexpr auto unpack_coeff0() const noexcept -> coeff_t { return coeffs_[0] >> 8; }
    constexpr auto unpack_log2_width() const noexcept -> int8_t { return static_cast<int8_t>(coeffs_[0].value & 0xFF); }

    [[no_unique_address]] fma_t fma_;
    [[no_unique_address]] shifter_t shifter_;
    coeffs_t coeffs_;
};

} // namespace crv::spline
