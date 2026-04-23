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
#include <new>

namespace crv::spline {

// fma with common types and fast nearest_up rounding mode
template <is_fixed in_t, is_fixed coeff_t>
using segment_fma_t = fma_t<coeff_t, in_t, coeff_t, coeff_t, shifter_t<rounding_modes::shr::fast::nearest_up_t>>;

/// fixed-point cubic spline segment packed into half a cache line
template <is_fixed t_in_t, is_fixed t_coeff_t, is_fixed t_out_t = t_coeff_t,
    typename fma_t = segment_fma_t<t_in_t, t_coeff_t>>
    requires(is_signed_v<typename t_coeff_t::value_t>)
class alignas(std::hardware_constructive_interference_size / 2) segment_t
{
public:
    using in_t = t_in_t;
    using out_t = t_out_t;
    using coeff_t = t_coeff_t;

    static_assert(sizeof(coeff_t) > 1); // must have room to pack dx_to_t_shift

    static constexpr auto coeff_count = 4;
    using coeffs_t = std::array<coeff_t, coeff_count>;

    constexpr segment_t(coeffs_t coeffs, int8_t dx_to_t_shift, fma_t fma = {}) noexcept
        : fma_{std::move(fma)}, coeffs_{coeffs}
    {
        // this type goes over the ioctl boundary, so it must be trivially copyable
        static_assert(std::is_trivially_copyable_v<segment_t>);

        // this type should fit into no more than half a cache line.
        static_assert(sizeof(segment_t) == 32); // nominal for x64
        static_assert(sizeof(segment_t) * 2 <= std::hardware_constructive_interference_size); // future architectures

        // make sure the top 8 bits of coeff[0] are clear so we can shift it and pack dx_to_t_shift in the bottom bits
        assert(coeffs[0] == ((coeffs[0] << 8) >> 8) && "segment_t: top 8 bits of first coefficient must be clear");

        // pack dx_to_t_shift into bottom 8 bits of coeff[0]
        coeffs_[0] <<= 8;
        coeffs_[0].value |= (static_cast<uint64_t>(dx_to_t_shift) & 0xFF);
    }

    // \pre 0 <= dx
    // \pre 0.0 <= shift(dx, dx_to_t_shift) < 1.0
    [[nodiscard]] constexpr auto operator()(in_t dx) const noexcept -> out_t
    {
        // unpack dx_to_t_shift from coeffs[0]
        auto const dx_to_t_shift = static_cast<int8_t>(coeffs_[0].value & 0xFF);
        auto const coeff0 = coeffs_[0] >> 8;

        auto const t = dx_to_t_shift < 0 ? (dx >> -dx_to_t_shift) : (dx << dx_to_t_shift);
        assert(in_t{0} <= dx);
        assert(t < in_t{1});

        auto result = coeff0;
        for (auto coeff = 1; coeff < coeff_count; ++coeff) result = fma_(result, t, coeffs_[coeff]);

        return out_t::convert(result);
    }

    [[nodiscard]] constexpr auto extend_final_tangent(in_t dx) const noexcept -> out_t
    {
        // unpack dx_to_t_shift from coeffs[0]
        auto const dx_to_t_shift = static_cast<int8_t>(coeffs_[0].value & 0xFF);
        auto const coeff0 = coeffs_[0] >> 8;

        // convert from dx to t
        auto const t = dx_to_t_shift < 0 ? (dx >> -dx_to_t_shift) : (dx << dx_to_t_shift);
        assert(in_t{0} <= dx);

        // p1 is the segment evaluated at t=1; the result is the same as the sum of coefficients
        auto const p1 = coeff0 + coeffs_[1] + coeffs_[2] + coeffs_[3];

        // final tangent is the derivative evaluated at t=1
        auto const m1 = 3 * coeff0 + 2 * coeffs_[1] + coeffs_[2];

        return out_t::convert(p1 + m1 * t);
    }

private:
    [[no_unique_address]] fma_t fma_;
    coeffs_t coeffs_;
};

} // namespace crv::spline
