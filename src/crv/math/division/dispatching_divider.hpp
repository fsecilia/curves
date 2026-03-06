// SPDX-License-Identifier: MIT

/// \file
/// \brief dispatches division to specific dividers
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/division/concepts.hpp>
#include <crv/math/division/result.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <climits>

namespace crv::division {

/// delegates to hardware divider when possible, falls back to long divider when necessary
///
/// The hardware divider is more efficient, but traps if the quotient does not fit into a narrow destination. The
/// long divider handles wide quotients correctly, but costs more to execute. This type range checks using a shift, then
/// dispatches to hardware when it can, to software when it must.
template <unsigned_integral dividend_t, unsigned_integral divisor_t,
          is_divider<dividend_t, divisor_t> hardware_divider_t, is_divider<dividend_t, divisor_t> long_divider_t>
struct dispatching_divider_t;

/// specializes explicitly on wide dividend, narrow divisor
template <unsigned_integral narrow_t, is_divider<wider_t<narrow_t>, narrow_t> hardware_divider_t,
          is_divider<wider_t<narrow_t>, narrow_t> long_divider_t>
struct dispatching_divider_t<wider_t<narrow_t>, narrow_t, hardware_divider_t, long_divider_t>
{
    using wide_t = wider_t<narrow_t>;

    [[no_unique_address]] hardware_divider_t hardware_divider;
    [[no_unique_address]] long_divider_t     long_divider;

    auto operator()(wide_t dividend, narrow_t divisor) const noexcept -> result_t<wide_t, narrow_t>
    {
        constexpr auto const narrow_width = sizeof(divisor) * CHAR_BIT;

        auto const quotient_fits = (dividend >> narrow_width) < int_cast<wide_t>(divisor);
        if (quotient_fits)
        {
            auto const hardware_result = hardware_divider(dividend, divisor);

            // promote hardware's narrow quotient to wide to match long divider
            return {.quotient  = int_cast<wide_t>(hardware_result.quotient),
                    .remainder = int_cast<narrow_t>(hardware_result.remainder)};
        }

        // long divider already has wide quotient
        return long_divider(dividend, divisor);
    }
};

} // namespace crv::division
