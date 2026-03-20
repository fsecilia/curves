// SPDX-License-Identifier: MIT

/// \file
/// \brief divider that applies rounding
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/division/concepts.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <crv/math/rounding_mode.hpp>
#include <cassert>

namespace crv::division {

/// brackets division with rounding mode
///
/// This type applies a rounding mode, biasing before for fast modes and carrying after for safe modes.
template <unsigned_integral narrow_t, is_wide_divider<narrow_t> wide_divider_t> struct rounding_divider_t
{
    using wide_t = wider_t<narrow_t>;

    [[no_unique_address]] wide_divider_t divide;

    /// applies rounding mode pre-bias, divides, then applies rounding mode post-carry
    template <is_div_rounding_mode<wide_t, narrow_t> rounding_mode_t>
    constexpr auto operator()(wide_t dividend, narrow_t divisor, rounding_mode_t rounding_mode) const noexcept -> wide_t
    {
        auto const biased = rounding_mode.bias(dividend, divisor);
        auto const result = divide(biased, divisor);
        return rounding_mode.carry(result.quotient, divisor, result.remainder);
    }
};

} // namespace crv::division
