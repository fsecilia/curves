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

namespace detail {

/// returns clz, clamped to one less than bit-width of integral, avoiding UB from `0 << clz(0)`
///
/// In general, dividends are normalized via `dividend << clz(dividend)`. However, if `dividend` is 0, `clz` returns the
/// full bit-width of integer type. This evaluates to `0 << width`, which invokes UB. By bitwise ORing in a 1,
/// `clz(dividend | 1)`, the count only changes for the `dividend == 0` case, where it returns one less than the full
/// bit-width, avoiding UB. Left shifting 0 is still 0, so the final result is correct in all cases. This saves a branch
/// at the cost of a const ALU op.
template <unsigned_integral src_t> constexpr auto safe_clz(src_t src) noexcept -> src_t
{
    return int_cast<src_t>(std::countl_zero(int_cast<src_t>(src | 1)));
}

} // namespace detail

/// divides a narrow dividend by narrow divisor, shifting, then returns rounded narrow quotient, optionally saturating
template <typename wide_divider_t, int total_shift, bool saturates = true> struct shifted_int_divider_t
{
    [[no_unique_address]] wide_divider_t divide;

    /// widens, shifts, delegates, optionally saturates, and narrows
    template <unsigned_integral narrow_t, typename rounding_mode_t>
        requires is_wide_divider<wide_divider_t, narrow_t, rounding_mode_t>
    constexpr auto operator()(narrow_t dividend, narrow_t divisor, rounding_mode_t rounding_mode) const noexcept
        -> narrow_t
    {
        using wide_t = wider_t<narrow_t>;

        // handle divide by 0
        if constexpr (saturates)
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

        auto const wide_dividend = int_cast<wide_t>(dividend);

        [[maybe_unused]] auto const headroom = detail::safe_clz(wide_dividend);
        assert(total_shift <= headroom && "crz::division::saturating_divider_t: pre-shift would overflow");

        auto const shifted_dividend = wide_dividend << total_shift;
        auto const wide_quotient    = divide(shifted_dividend, divisor, rounding_mode);

        if constexpr (saturates)
        {
            if (wide_quotient > int_cast<wide_t>(max<narrow_t>())) return max<narrow_t>();
        }

        return static_cast<narrow_t>(wide_quotient);
    }

    /// strips sign, widens, shifts, delegates, optionally saturates, narrows, and restores sign.
    template <signed_integral narrow_t, typename rounding_mode_t>
        requires is_wide_divider<wide_divider_t, make_unsigned_t<narrow_t>, rounding_mode_t>
    constexpr auto operator()(narrow_t dividend, narrow_t divisor, rounding_mode_t rounding_mode) const noexcept
        -> narrow_t
    {
        using unsigned_t = make_unsigned_t<narrow_t>;
        using wide_t     = wider_t<unsigned_t>;

        // handle divide by 0
        if constexpr (saturates)
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

        auto const abs_dividend = static_cast<unsigned_t>(dividend < 0 ? -static_cast<unsigned_t>(dividend)
                                                                       : static_cast<unsigned_t>(dividend));
        auto const abs_divisor  = static_cast<unsigned_t>(divisor < 0 ? -static_cast<unsigned_t>(divisor)
                                                                      : static_cast<unsigned_t>(divisor));

        auto const wide_dividend = int_cast<wide_t>(abs_dividend);

        [[maybe_unused]] auto const headroom = detail::safe_clz(wide_dividend);
        assert(total_shift <= headroom && "crz::division::saturating_divider_t: pre-shift would overflow");

        auto const shifted_dividend = wide_dividend << total_shift;

        auto const negative      = (dividend ^ divisor) < 0;
        auto const wide_quotient = divide(shifted_dividend, abs_divisor, rounding_mode);

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
