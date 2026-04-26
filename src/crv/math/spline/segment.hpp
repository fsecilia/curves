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
    /// \pre dx < (1 << log2_width)
    [[nodiscard]] constexpr auto evaluate(x_t dx) const noexcept -> y_t
    {
        auto const t = dx_to_t(dx);
        assert(t < x_t{1}); // dx < (1 << log2_width)

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
        auto const final_tangent = calc_final_tangent();
        return y_t::convert(fma_(coeff_t::convert(final_tangent.m1), t, coeff_t::convert(final_tangent.p1)));
    }

    /// validates segment data
    constexpr auto is_valid() const noexcept -> bool
    {
        // maximum valid shift for the input type
        static constexpr auto max_shift = static_cast<int8_t>(sizeof(typename x_t::value_t) * CHAR_BIT);

        // shift amount must be within (-max_shift, max_shift)
        auto const log2_width = unpack_log2_width();
        if (log2_width <= -max_shift || max_shift <= log2_width) return false;

        auto const final_tangent = calc_final_tangent();
        if (final_tangent.p1 < wide_t::convert(min<y_t>()) || wide_t::convert(max<y_t>()) < final_tangent.p1
            || final_tangent.m1 < wide_t::convert(min<coeff_t>()) || wide_t::convert(max<coeff_t>()) < final_tangent.m1)
        {
            return false;
        }

        return true;
    }

private:
    using wide_t = fixed_t<widened_t<typename coeff_t::value_t>, coeff_t::frac_bits>;

    struct final_tangent_t
    {
        wide_t p1;
        wide_t m1;
    };

    [[nodiscard]] constexpr auto calc_final_tangent() const noexcept -> final_tangent_t
    {
        auto const c0 = wide_t::convert(unpack_coeff0());
        auto const c1 = wide_t::convert(coeffs_[1]);
        auto const c2 = wide_t::convert(coeffs_[2]);
        auto const c3 = wide_t::convert(coeffs_[3]);

        return {
            .p1 = c0 + c1 + c2 + c3, // segment evaluated at t=1; 1^n = 1, so result is same as sum of coefficients
            .m1 = 3 * c0 + 2 * c1 + c2 // derivative evaluated at t=1: 3*c0*t^2 + 2*c1*t + c2|t=1 -> 3*c0 + 2*c1 + c2
        };
    }

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
