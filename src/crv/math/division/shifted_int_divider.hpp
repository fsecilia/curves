// SPDX-License-Identifier: MIT

/// \file
/// \brief integer divider with a preshift
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/division/concepts.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/rounding_mode.hpp>
#include <cassert>

namespace crv::division {

template <typename narrow_divider_t, bool saturates = true> struct shifted_int_divider_t
{
    [[no_unique_address]] narrow_divider_t narrow_divider;

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

    template <signed_integral narrow_t, typename rounding_mode_t>
        requires is_narrow_divider<narrow_divider_t, make_unsigned_t<narrow_t>, rounding_mode_t>
    constexpr auto operator()(narrow_t dividend, narrow_t divisor, rounding_mode_t rounding_mode) const noexcept
        -> narrow_t
    {
        using unsigned_t = make_unsigned_t<narrow_t>;
        using wide_t     = wider_t<unsigned_t>;

        auto const negative     = (dividend ^ divisor) < 0;
        auto const abs_dividend = dividend < 0 ? -static_cast<unsigned_t>(dividend) : static_cast<unsigned_t>(dividend);
        auto const abs_divisor  = divisor < 0 ? -static_cast<unsigned_t>(divisor) : static_cast<unsigned_t>(divisor);

        auto const wide_quotient = narrow_divider(abs_dividend, abs_divisor, rounding_mode);
        auto const magnitude     = static_cast<unsigned_t>(wide_quotient);
        auto const signed_result = negative ? static_cast<narrow_t>(-magnitude) : static_cast<narrow_t>(magnitude);

        if constexpr (saturates)
        {
            // negative side has 1 more value than positive side (e.g. [-128, 127]); asymmetric_bound is that magnitude
            auto const asymmetric_bound = static_cast<wide_t>(max<narrow_t>()) + static_cast<wide_t>(negative);
            auto const saturated_result = negative ? min<narrow_t>() : max<narrow_t>();
            return (wide_quotient > asymmetric_bound) ? saturated_result : signed_result;
        }

        return signed_result;
    }
};

} // namespace crv::division
