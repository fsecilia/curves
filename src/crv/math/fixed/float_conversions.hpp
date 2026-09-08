// SPDX-License-Identifier: MIT

/// \file
/// \brief float conversions for fixed_t
///
/// There's no float in the kernel, only in user mode, so instead of wrapping float conversions with !__KERNEL__, they
/// have been extracted to a dedicated module.
///
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/int_traits.hpp>
#include <cmath>
#include <concepts>
#include <limits>

namespace crv {

/// converts to and from fixed_t
template <typename fixed_t> struct fixed_converter_t;

/// specialization to crack target's parameters
template <integral value_t, int frac_bits> struct fixed_converter_t<fixed_t<value_t, frac_bits>>
{
    using target_t = fixed_t<value_t, frac_bits>;

    /// checks whether conversion can represent the rounded source exactly in target's integer storage
    template <std::floating_point src_t> constexpr auto is_representable(src_t src) const noexcept -> bool
    {
        using std::isfinite;
        using std::ldexp;
        using std::rint;

        auto const scaled = ldexp(src, frac_bits);
        if (!isfinite(scaled)) return false;

        auto const rounded = rint(scaled);
        auto const limit = ldexp(src_t{1}, std::numeric_limits<value_t>::digits);
        if constexpr (is_signed_v<value_t>) return rounded >= -limit && rounded < limit;
        else return rounded >= src_t{0} && rounded < limit;
    }

    template <std::floating_point src_t> constexpr auto to(src_t src) const noexcept -> target_t
    {
        using std::ldexp;
        using std::rint;

        assert(is_representable(src) && "fixed_converter_t::to: input out of range");
        auto const rounded = rint(ldexp(src, frac_bits));
        return target_t::literal(static_cast<value_t>(rounded));
    }

    template <std::floating_point dst_t> constexpr auto from(target_t src) const noexcept -> dst_t
    {
        using std::ldexp;

        return ldexp(static_cast<dst_t>(src.value), -frac_bits);
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
