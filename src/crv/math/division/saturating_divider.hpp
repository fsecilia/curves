// SPDX-License-Identifier: MIT

/// \file
/// \brief divider that optionally saturates before narrowing wide quotient
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/division/concepts.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/rounding_mode.hpp>
#include <cassert>

namespace crv::division {

/// divides a narrow dividend by narrow divisor, then returns rounded narrow quotient, optionally saturating
template <typename narrow_divider_t, bool saturates = true> struct saturating_divider_t
{
    [[no_unique_address]] narrow_divider_t narrow_divider;

    /// widens, delegates, optionally saturates, and narrows
    template <unsigned_integral narrow_t, typename rounding_mode_t>
        requires is_narrow_divider<narrow_divider_t, narrow_t, rounding_mode_t>
    constexpr auto operator()(narrow_t dividend, narrow_t divisor, rounding_mode_t rounding_mode) const noexcept
        -> narrow_t
    {
        using wide_t = wider_t<narrow_t>;

        auto const wide_quotient = narrow_divider(dividend, divisor, rounding_mode);

        if constexpr (saturates)
        {
            if (wide_quotient > int_cast<wide_t>(max<narrow_t>())) return max<narrow_t>();
        }

        return static_cast<narrow_t>(wide_quotient);
    }

    /// strips sign, widens, delegates, optionally saturates, narrows, and restores sign.
    template <signed_integral narrow_t, typename rounding_mode_t>
        requires is_narrow_divider<narrow_divider_t, make_unsigned_t<narrow_t>, rounding_mode_t>
    constexpr auto operator()(narrow_t dividend, narrow_t divisor, rounding_mode_t rounding_mode) const noexcept
        -> narrow_t
    {
        using unsigned_t = make_unsigned_t<narrow_t>;
        using wide_t     = wider_t<unsigned_t>;

        auto const abs_dividend = static_cast<unsigned_t>(dividend < 0 ? -static_cast<unsigned_t>(dividend)
                                                                       : static_cast<unsigned_t>(dividend));
        auto const abs_divisor  = static_cast<unsigned_t>(divisor < 0 ? -static_cast<unsigned_t>(divisor)
                                                                      : static_cast<unsigned_t>(divisor));

        auto const negative      = (dividend ^ divisor) < 0;
        auto const wide_quotient = narrow_divider(abs_dividend, abs_divisor, rounding_mode);

        if constexpr (saturates)
        {
            // negative side has 1 more value than positive side (e.g. [-128, 127]); asymmetric_bound is that magnitude
            auto const asymmetric_bound = static_cast<wide_t>(max<narrow_t>()) + static_cast<wide_t>(negative);
            if (wide_quotient > asymmetric_bound) { return negative ? min<narrow_t>() : max<narrow_t>(); }
        }

        // calc narrow signed result only if we know it fits, or when truncating rather than saturating
        auto const magnitude = static_cast<unsigned_t>(wide_quotient);
        return negative ? static_cast<narrow_t>(-magnitude) : static_cast<narrow_t>(magnitude);
    }
};

} // namespace crv::division
