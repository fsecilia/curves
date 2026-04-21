// SPDX-License-Identifier: MIT

/// \file
/// \brief scaling integer divider with optional saturation
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/cmp.hpp>
#include <crv/math/division/concepts.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/rounding_mode.hpp>
#include <algorithm>
#include <cassert>

namespace crv::division {

/// divides, scales, rounds and saturates integers via wide intermediate
template <typename wide_divider_t, int shift, integral out_value_t, integral lhs_t, integral rhs_t,
    bool saturate = true>
struct shifted_int_divider_t;

/// strictly unsigned division
template <typename wide_divider_t, int shift, integral out_value_t, unsigned_integral lhs_t, unsigned_integral rhs_t,
    bool saturate>
struct shifted_int_divider_t<wide_divider_t, shift, out_value_t, lhs_t, rhs_t, saturate>
{
    [[no_unique_address]] wide_divider_t divide;

    static constexpr auto narrow_size = std::max(sizeof(lhs_t), sizeof(rhs_t));

    using narrow_t = int_by_bytes_t<narrow_size, false>;
    using wide_t = widened_t<narrow_t>;

    static constexpr auto max_wide_pre_shift = max<wide_t>() >> shift;

    static constexpr auto max_possible_dividend = int_cast<wide_t>(max<lhs_t>());
    static constexpr auto max_possible_quotient = int_cast<wide_t>(max_possible_dividend << shift);
    static constexpr bool upper_saturation_possible = cmp_greater(max_possible_quotient, max<out_value_t>());

    template <typename rounding_mode_t>
    constexpr auto operator()(lhs_t dividend, rhs_t divisor, rounding_mode_t rounding_mode) const noexcept
        -> out_value_t
    {
        // handle divide by 0
        if constexpr (saturate)
        {
            if (divisor == 0) [[unlikely]]
            {
                if (dividend == 0) return out_value_t{0};
                return max<out_value_t>();
            }
        }
        else
        {
            assert(divisor != 0);
        }

        // widen
        auto const wide_dividend = int_cast<wide_t>(dividend);
        assert(wide_dividend <= max_wide_pre_shift && "crv::division::shifted_int_divider_t: pre-shift would overflow");

        // shift
        auto const shifted_dividend = int_cast<wide_t>(wide_dividend << shift);

        // divide
        auto const wide_quotient = divide(shifted_dividend, static_cast<narrow_t>(divisor), rounding_mode);

        // optionally saturate
        if constexpr (saturate && upper_saturation_possible)
        {
            if (cmp_greater(wide_quotient, max<out_value_t>())) return max<out_value_t>();
        }

        // return narrowed quotient
        return static_cast<out_value_t>(wide_quotient);
    }
};

/// mixed-sign division
template <typename wide_divider_t, int shift, integral out_value_t, integral lhs_t, integral rhs_t, bool saturate>
    requires(std::is_signed_v<lhs_t> || std::is_signed_v<rhs_t>)
struct shifted_int_divider_t<wide_divider_t, shift, out_value_t, lhs_t, rhs_t, saturate>
{
    [[no_unique_address]] wide_divider_t divide;

    static constexpr auto narrow_size = std::max(sizeof(lhs_t), sizeof(rhs_t));

    using narrow_t = int_by_bytes_t<narrow_size, false>;
    using wide_t = widened_t<narrow_t>;
    using wide_signed_t = make_signed_t<wide_t>;

    static constexpr bool lhs_can_be_negative = std::is_signed_v<lhs_t>;
    static constexpr bool rhs_can_be_negative = std::is_signed_v<rhs_t>;

    static constexpr auto max_wide_pre_shift = max<wide_t>() >> shift;

    static constexpr wide_t max_abs_dividend = lhs_can_be_negative
        ? int_cast<wide_t>(static_cast<make_unsigned_t<lhs_t>>(min<lhs_t>()))
        : int_cast<wide_t>(max<lhs_t>());

    static constexpr wide_t max_shifted_dividend = int_cast<wide_t>(max_abs_dividend << shift);
    static constexpr bool upper_saturation_possible = cmp_greater(max_shifted_dividend, max<out_value_t>());

    static constexpr wide_t max_abs_out_min = std::is_signed_v<out_value_t>
        ? static_cast<wide_t>(static_cast<make_unsigned_t<out_value_t>>(min<out_value_t>()))
        : wide_t{0};

    static constexpr bool lower_saturation_possible
        = std::is_unsigned_v<out_value_t> ? true : (max_shifted_dividend > max_abs_out_min);

    template <typename rounding_mode_t>
    constexpr auto operator()(lhs_t dividend, rhs_t divisor, rounding_mode_t rounding_mode) const noexcept
        -> out_value_t
    {
        // handle divide by 0
        if constexpr (saturate)
        {
            if (divisor == 0) [[unlikely]]
            {
                if (dividend == 0) return out_value_t{0};
                return dividend > 0 ? max<out_value_t>() : min<out_value_t>();
            }
        }
        else
        {
            assert(divisor != 0);
        }

        // strip sign
        bool const lhs_negative = lhs_can_be_negative ? cmp_less(dividend, 0) : false;
        bool const rhs_negative = rhs_can_be_negative ? cmp_less(divisor, 0) : false;
        bool const negative = lhs_negative != rhs_negative;

        auto const abs_dividend
            = static_cast<narrow_t>(lhs_negative ? -static_cast<narrow_t>(dividend) : static_cast<narrow_t>(dividend));
        auto const abs_divisor
            = static_cast<narrow_t>(rhs_negative ? -static_cast<narrow_t>(divisor) : static_cast<narrow_t>(divisor));

        // widen
        auto const wide_dividend = int_cast<wide_t>(abs_dividend);
        assert(wide_dividend <= max_wide_pre_shift && "crv::division::shifted_int_divider_t: pre-shift would overflow");

        // shift
        auto const shifted_dividend = int_cast<wide_t>(wide_dividend << shift);

        // divide
        auto const wide_quotient = divide(shifted_dividend, abs_divisor, rounding_mode);

        // restore sign
        auto const signed_result
            = negative ? -static_cast<wide_signed_t>(wide_quotient) : static_cast<wide_signed_t>(wide_quotient);

        // optionally saturate
        if constexpr (saturate)
        {
            if constexpr (lower_saturation_possible)
            {
                if (cmp_less(signed_result, min<out_value_t>())) return min<out_value_t>();
            }
            if constexpr (upper_saturation_possible)
            {
                if (cmp_greater(signed_result, max<out_value_t>())) return max<out_value_t>();
            }
        }

        return static_cast<out_value_t>(signed_result);
    }
};

} // namespace crv::division
