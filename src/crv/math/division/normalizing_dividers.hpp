// SPDX-License-Identifier: MIT

/// \file
/// \brief division normalization strategies
/// \copyright Copyright (C) 2026 Frank Secilia
///
/// Normalization widens and pre-shifts the dividend left before dividing, delegates division, then shifts the wide
/// quotient into place with rounding applied.

#pragma once

#include <crv/lib.hpp>
#include <crv/math/division/concepts.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <algorithm>
#include <bit>
#include <cassert>

namespace crv::division::normalizing_dividers {

namespace detail {

/// returns clz, clamped to one less than bit-width of integral, avoiding UB from `0 << safe_clz(0)`
///
/// In general, dividends are normalized via `dividend << clz(dividend)`. However, if `dividend` is 0, `clz` returns the
/// full bit-width of integer type. This evaluates to `0 << width`, which invokes UB. By bitwise ORing in a 1,
/// `clz(dividend | 1)`, the count only changes for the `dividend == 0` case, where it returns one less than the full
/// bit-width, avoiding UB. Left shifting 0 is still 0, so the final result is correct in all cases. This saves a branch
/// at the cost of a const ALU op.
template <integral integral_t> constexpr auto safe_clz(integral_t src) noexcept -> integral_t
{
    return int_cast<integral_t>(std::countl_zero(int_cast<integral_t>(src | 1)));
}

} // namespace detail

/// normalizes dynamically to maximize pre shift from a variable dividend
template <unsigned_integral narrow_t, int total_shift, is_wide_divider<narrow_t> wide_divider_t> struct variable_t
{
    using wide_t = wider_t<narrow_t>;

    [[no_unique_address]] wide_divider_t divide;

    template <typename mode_t>
    constexpr auto operator()(narrow_t dividend, narrow_t divisor, mode_t mode) const noexcept -> wide_t
    {
        auto const wide_dividend = int_cast<wide_t>(dividend);

        auto const pre_shift  = detail::safe_clz(wide_dividend);
        auto const post_shift = pre_shift - total_shift;

        // Left shift after divide is unlikely, but handling it has a cost. It is unsupported until necessary.
        assert(post_shift >= 0
               && "crz::division::normalizer::variable_t::operator(): left shift after divide not supported");

        auto const shifted_dividend = wide_dividend << pre_shift;
        auto const result           = divide(shifted_dividend, divisor);

        auto const shifted_quotient = result.quotient >> post_shift;
        return mode.div_shr(shifted_quotient, result.quotient, divisor, result.remainder, post_shift);
    }
};

/// normalizes statically to maximize pre shift from a known constant dividend
template <unsigned_integral narrow_t, narrow_t constant_dividend, int total_shift,
          is_wide_divider<narrow_t> wide_divider_t>
struct constant_t
{
    using wide_t = wider_t<narrow_t>;

    [[no_unique_address]] wide_divider_t divide;

    static constexpr auto headroom   = detail::safe_clz(static_cast<wide_t>(constant_dividend));
    static constexpr auto pre_shift  = std::min(int_cast<wide_t>(total_shift), headroom);
    static constexpr auto post_shift = pre_shift - total_shift;

    // Left shift after divide is unlikely, but handling it has a cost. It is unsupported until necessary.
    static_assert(post_shift <= 0, "crz::division::normalizer::constant_t: left shift after divide not supported");

    template <typename mode_t>
    constexpr auto operator()(narrow_t dividend, narrow_t divisor, mode_t mode) const noexcept -> wide_t
    {
        assert(dividend == constant_dividend
               && "crz::division::normalizer::constant_t::operator(): runtime dividend must match specified constant");

        constexpr auto wide_dividend    = int_cast<wide_t>(constant_dividend);
        constexpr auto shifted_dividend = wide_dividend << pre_shift;

        // fast path is available when there is at least one bit of headroom to hold bias; implies no post shift
        constexpr auto can_use_fast_path = pre_shift < headroom;
        if constexpr (can_use_fast_path)
        {
            auto const biased_dividend = mode.div_bias(shifted_dividend, divisor);
            auto const result          = divide(biased_dividend, divisor);
            return mode.div_carry(result.quotient, divisor, result.remainder);
        }
        else
        {
            auto const result           = divide(shifted_dividend, divisor);
            auto const shifted_quotient = result.quotient >> -post_shift;
            return mode.div_shr(shifted_quotient, result.quotient, divisor, result.remainder, -post_shift);
        }
    }
};

} // namespace crv::division::normalizing_dividers
