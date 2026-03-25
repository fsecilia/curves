// SPDX-License-Identifier: MIT

/// \file
/// \brief scaling integer divider with optional saturation
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/division/concepts.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/rounding_mode.hpp>
#include <cassert>

namespace crv::division {

namespace detail {

/// clz, safely handling input 0
///
/// ORs 1 into LSB before calling clz to limit max return to width - 1. This prevents UB when passing 0. Since shifting
/// 0 left by any value is still 0, the final result is still correct in all cases. This saves a branch at the cost of a
/// const ALU op.
template <unsigned_integral src_t> constexpr auto safe_clz(src_t src) noexcept -> src_t
{
    return int_cast<src_t>(std::countl_zero(int_cast<src_t>(src | 1)));
}

} // namespace detail

/// divides, scales, and rounds narrow integers via wide intermediate
///
/// Widens dividend, applies shift, divides via wide_divider_t, optionally saturates, and narrows.
template <typename wide_divider_t, int shift, bool saturate = true> struct shifted_int_divider_t
{
    [[no_unique_address]] wide_divider_t divide;

    /// unsigned division
    ///
    /// Handles zero-division gracefully if saturating. Asserts headroom before shifting.
    template <unsigned_integral narrow_t, typename rounding_mode_t>
        requires is_wide_divider<wide_divider_t, narrow_t, rounding_mode_t>
    constexpr auto operator()(narrow_t dividend, narrow_t divisor, rounding_mode_t rounding_mode) const noexcept
        -> narrow_t
    {
        using wide_t = wider_t<narrow_t>;

        // handle divide by 0
        if constexpr (saturate)
        {
            if (divisor == 0) [[unlikely]]
            {
                if (dividend == 0) return narrow_t{0};
                return max<narrow_t>();
            }
        }
        else
        {
            assert(divisor != 0);
        }

        // widen, shift, divide
        auto const wide_dividend = int_cast<wide_t>(dividend);
        assert(shift <= detail::safe_clz(wide_dividend)
               && "crz::division::saturating_divider_t: pre-shift would overflow");
        auto const wide_quotient = divide(wide_dividend << shift, divisor, rounding_mode);

        // handle saturation
        if constexpr (saturate)
        {
            if (wide_quotient > int_cast<wide_t>(max<narrow_t>())) return max<narrow_t>();
        }

        // return narrowed quotient
        return static_cast<narrow_t>(wide_quotient);
    }

    /// signed division
    ///
    /// Strips sign, processes as unsigned wide integers, applies two's complement asymmetric bounds checking if
    /// saturating, and restores sign upon narrowing. Prevents UB on INT_MIN negation.
    template <signed_integral narrow_t, typename rounding_mode_t>
        requires is_wide_divider<wide_divider_t, make_unsigned_t<narrow_t>, rounding_mode_t>
    constexpr auto operator()(narrow_t dividend, narrow_t divisor, rounding_mode_t rounding_mode) const noexcept
        -> narrow_t
    {
        using unsigned_t = make_unsigned_t<narrow_t>;
        using wide_t     = wider_t<unsigned_t>;

        // handle divide by 0
        if constexpr (saturate)
        {
            if (divisor == 0) [[unlikely]]
            {
                if (dividend == 0) return narrow_t{0};
                return dividend > 0 ? max<narrow_t>() : min<narrow_t>();
            }
        }
        else
        {
            assert(divisor != 0);
        }

        // strip sign
        auto const abs_dividend = static_cast<unsigned_t>(dividend < 0 ? -static_cast<unsigned_t>(dividend)
                                                                       : static_cast<unsigned_t>(dividend));
        auto const abs_divisor  = static_cast<unsigned_t>(divisor < 0 ? -static_cast<unsigned_t>(divisor)
                                                                      : static_cast<unsigned_t>(divisor));

        auto const wide_dividend = int_cast<wide_t>(abs_dividend);
        assert(shift <= detail::safe_clz(wide_dividend)
               && "crz::division::saturating_divider_t: pre-shift would overflow");

        auto const negative      = (dividend ^ divisor) < 0;
        auto const wide_quotient = divide(wide_dividend << shift, abs_divisor, rounding_mode);

        if constexpr (saturate)
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
