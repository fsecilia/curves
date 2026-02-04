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
template <integral value_t, int frac_bits> struct fixed_converter_t<fixed_t<value_t, frac_bits>>
{
    using target_t = fixed_t<value_t, frac_bits>;

    template <std::floating_point src_t> constexpr auto to(src_t src) const noexcept -> target_t
    {
        return target_t{static_cast<value_t>(std::round(src * std::ldexp(src_t{1}, frac_bits)))};
    }

    template <std::floating_point dst_t> constexpr auto from(target_t src) const noexcept -> dst_t
    {
        return static_cast<dst_t>(src.value) / std::ldexp(dst_t{1}, frac_bits);
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
