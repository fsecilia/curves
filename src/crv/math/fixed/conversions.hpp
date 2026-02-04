// SPDX-License-Identifier: MIT
/*!
    \file
    \brief io for fixed_t

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/int_traits.hpp>
#include <cmath>
#include <concepts>

namespace crv {

/// converts to and from fixed_t
template <typename fixed_t> struct fixed_converter_t;

/// specialization to crack target's parameters
template <integral value_t, int_t frac_bits> struct fixed_converter_t<fixed_t<value_t, frac_bits>>
{
    using target_t = fixed_t<value_t, frac_bits>;

#ifndef NDEBUG
    template <std::floating_point src_t> constexpr auto range_check(src_t scaled) const noexcept -> void
    {
        if constexpr (std::is_signed_v<value_t>)
        {
            auto const limit = ldexp(src_t{1}, std::numeric_limits<value_t>::digits);
            assert(scaled >= -limit && scaled < limit && "fixed_converter_t::to: input out of range");
        }
        else
        {
            auto const limit = ldexp(src_t{1}, std::numeric_limits<value_t>::digits);
            assert(scaled >= 0 && scaled < limit && "fixed_converter_t::to: input out of range");
        }
    }
#else
    template <std::floating_point src_t> constexpr auto range_check(src_t) const noexcept -> void {}
#endif

    template <std::floating_point src_t> constexpr auto to(src_t src) const noexcept -> target_t
    {
        using std::ldexp;
        using std::llround;
        using std::round;

        auto const scaled = ldexp(src, frac_bits);
        range_check(scaled);

        // dispatch with llround when possible
        static constexpr auto llround_available
            = std::numeric_limits<value_t>::max() <= std::numeric_limits<long long>::max();
        if constexpr (llround_available) { return target_t{static_cast<value_t>(llround(scaled))}; }
        else
        {
            return target_t{static_cast<value_t>(round(scaled))};
        }
    }

    template <std::floating_point dst_t> constexpr auto from(target_t src) const noexcept -> dst_t
    {
        using std::ldexp;
        return static_cast<dst_t>(src.value) / ldexp(dst_t{1}, frac_bits);
    }
};

/// converts to fixed from any float type
template <typename dst_t, std::floating_point src_t> constexpr auto to_fixed(src_t src) noexcept -> dst_t
{
    return fixed_converter_t<dst_t>{}.to(src);
}

/// converts from fixed to given float type
template <std::floating_point dst_t, typename src_t> constexpr auto from_fixed(src_t src) noexcept -> dst_t
{
    return fixed_converter_t<src_t>{}.template from<dst_t>(src);
}

} // namespace crv
