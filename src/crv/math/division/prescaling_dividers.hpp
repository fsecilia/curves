// SPDX-License-Identifier: MIT

/// \file
/// \brief strategies for widening and prescaling dividend before division
/// \copyright Copyright (C) 2026 Frank Secilia
///
/// This module provides strategies to widen the dividend, then prescale it to increase precision.

#pragma once

#include <crv/lib.hpp>
#include <crv/math/division/concepts.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/rounding_mode.hpp>
#include <bit>
#include <cassert>

namespace crv::division::prescaling_dividers {
namespace detail {

/// returns clz, clamped to one less than bit-width of integral, avoiding UB from `0 << clz(0)`
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

/// dynamically pre-shifts a variable dividend
template <unsigned_integral narrow_t, int total_shift, typename divider_t> struct variable_t
{
    using wide_t = wider_t<narrow_t>;

    [[no_unique_address]] divider_t divide;

    template <is_div_rounding_mode<wide_t, narrow_t> rounding_mode_t>
        requires(is_wide_divider<divider_t, narrow_t, rounding_mode_t>)
    constexpr auto operator()(narrow_t dividend, narrow_t divisor, rounding_mode_t rounding_mode) const noexcept
        -> wide_t
    {
        [[maybe_unused]] auto const headroom = detail::safe_clz(int_cast<wide_t>(dividend));
        assert(total_shift <= headroom && "crz::division::normalizer::variable_t: pre-shift would overflow");

        auto const wide_dividend    = int_cast<wide_t>(dividend);
        auto const shifted_dividend = wide_dividend << total_shift;
        return divide(shifted_dividend, divisor, rounding_mode);
    }
};

/// statically pre-shifts a known constant dividend
template <unsigned_integral narrow_t, narrow_t constant_dividend, int total_shift, typename divider_t> struct constant_t
{
    using wide_t = wider_t<narrow_t>;

    [[no_unique_address]] divider_t divide;

    static constexpr auto headroom = detail::safe_clz(int_cast<wide_t>(constant_dividend));
    static_assert(total_shift <= headroom, "crz::division::prescaling_dividers::constant_t: pre-shift would overflow");

    static constexpr auto wide_dividend    = int_cast<wide_t>(constant_dividend);
    static constexpr auto shifted_dividend = wide_dividend << total_shift;

    template <is_div_rounding_mode<wide_t, narrow_t> rounding_mode_t>
        requires(is_wide_divider<divider_t, narrow_t, rounding_mode_t>)
    constexpr auto operator()([[maybe_unused]] narrow_t dividend, narrow_t divisor,
                              rounding_mode_t rounding_mode) const noexcept -> wide_t
    {
        assert(
            dividend == constant_dividend
            && "crz::division::prescaling_dividers::constant_t::operator(): runtime dividend must match specified constant");

        return divide(shifted_dividend, divisor, rounding_mode);
    }
};

} // namespace crv::division::prescaling_dividers
