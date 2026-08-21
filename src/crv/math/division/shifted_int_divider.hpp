// SPDX-License-Identifier: MIT

/// \file
/// \brief scaling integer divider with optional saturation
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/cmp.hpp>
#include <crv/math/division/concepts.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/rounding_mode.hpp>
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

    using narrow_t = wide_divider_t::narrow_t;
    using wide_t = wide_divider_t::wide_t;

    static_assert(
        sizeof(lhs_t) <= sizeof(wide_t), "dividend is wider than the two-word dividend supported by the divisor");
    static_assert(sizeof(rhs_t) <= sizeof(narrow_t), "divisor is wider than the selected native division word");

    static constexpr auto max_wide_pre_shift = max<wide_t>() >> shift;

    static constexpr auto max_possible_dividend = int_cast<wide_t>(max<lhs_t>());
    static constexpr auto max_possible_quotient = [] {
        if constexpr (max_possible_dividend > max_wide_pre_shift) return max<wide_t>();
        else return int_cast<wide_t>(max_possible_dividend << shift);
    }();
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

    using narrow_t = wide_divider_t::narrow_t;
    using wide_t = wide_divider_t::wide_t;

    static_assert(
        sizeof(lhs_t) <= sizeof(wide_t), "dividend is wider than the two-word dividend supported by the divisor");
    static_assert(sizeof(rhs_t) <= sizeof(narrow_t), "divisor is wider than the selected native division word");

    using unsigned_lhs_t = make_unsigned_t<lhs_t>;
    using unsigned_rhs_t = make_unsigned_t<rhs_t>;
    using unsigned_out_t = make_unsigned_t<out_value_t>;

    static constexpr auto max_wide_pre_shift = max<wide_t>() >> shift;

    static constexpr auto max_abs_dividend = []() -> wide_t {
        // in two's-complement, converting the minimum signed value to unsigned produces its positive magnitude
        if constexpr (std::is_signed_v<lhs_t>) return int_cast<wide_t>(static_cast<unsigned_lhs_t>(min<lhs_t>()));
        else return int_cast<wide_t>(max<lhs_t>());
    }();

    static constexpr auto max_shifted_dividend = []() -> wide_t {
        if constexpr (max_abs_dividend > max_wide_pre_shift) return max<wide_t>();
        else return int_cast<wide_t>(max_abs_dividend << shift);
    }();

    static constexpr auto max_abs_out_min = []() -> wide_t {
        if constexpr (std::is_signed_v<out_value_t>)
        {
            return int_cast<wide_t>(static_cast<unsigned_out_t>(min<out_value_t>()));
        }
        else return wide_t{};
    }();

    static constexpr bool upper_saturation_possible = cmp_greater(max_shifted_dividend, max<out_value_t>());

    static constexpr bool lower_saturation_possible
        = std::is_unsigned_v<out_value_t> || (max_shifted_dividend >= max_abs_out_min);

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

        // strip signs
        bool const lhs_negative = std::is_signed_v<lhs_t> ? cmp_less(dividend, 0) : false;
        bool const rhs_negative = std::is_signed_v<rhs_t> ? cmp_less(divisor, 0) : false;
        bool const negative = lhs_negative != rhs_negative;

        // dividend may use the wide type
        auto const abs_dividend = [&]() -> wide_t {
            if constexpr (std::is_signed_v<lhs_t>)
            {
                auto const bits = static_cast<unsigned_lhs_t>(dividend);
                auto const magnitude = lhs_negative ? static_cast<unsigned_lhs_t>(unsigned_lhs_t{} - bits) : bits;
                return int_cast<wide_t>(magnitude);
            }
            else
            {
                return int_cast<wide_t>(dividend);
            }
        }();

        // divisor must fit in narrow word
        auto const abs_divisor = [&]() -> narrow_t {
            if constexpr (std::is_signed_v<rhs_t>)
            {
                auto const bits = static_cast<unsigned_rhs_t>(divisor);
                auto const magnitude = rhs_negative ? static_cast<unsigned_rhs_t>(unsigned_rhs_t{} - bits) : bits;
                return int_cast<narrow_t>(magnitude);
            }
            else
            {
                return int_cast<narrow_t>(divisor);
            }
        }();

        // shift
        assert(abs_dividend <= max_wide_pre_shift && "crv::division::shifted_int_divider_t: pre-shift would overflow");

        auto const shifted_dividend = int_cast<wide_t>(abs_dividend << shift);

        // divide
        auto const wide_quotient = divide(shifted_dividend, abs_divisor, rounding_mode);

        // restore sign and optionally saturate
        if constexpr (saturate)
        {
            if (negative)
            {
                if constexpr (std::is_unsigned_v<out_value_t>) return min<out_value_t>();
                else
                {
                    // >= deliberately includes the exactly representable minimum value. Returning min directly avoids
                    // trying to materialize its positive magnitude in out_value_t before negating it.
                    if constexpr (lower_saturation_possible)
                    {
                        if (wide_quotient >= max_abs_out_min) return min<out_value_t>();
                    }

                    return static_cast<out_value_t>(-static_cast<out_value_t>(wide_quotient));
                }
            }
            else
            {
                if constexpr (upper_saturation_possible)
                {
                    if (cmp_greater(wide_quotient, max<out_value_t>())) return max<out_value_t>();
                }

                return static_cast<out_value_t>(wide_quotient);
            }
        }
        else
        {
            auto const magnitude = static_cast<unsigned_out_t>(wide_quotient);
            auto const result = negative ? static_cast<unsigned_out_t>(unsigned_out_t{} - magnitude) : magnitude;
            return static_cast<out_value_t>(result);
        }
    }
};

} // namespace crv::division
