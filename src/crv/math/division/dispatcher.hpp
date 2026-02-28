// SPDX-License-Identifier: MIT
/**
    \file
    \brief dispatches division to specific dividers

    \copyright Copyright (C) 2026 Frank Secilia
*/

#pragma once

#include <crv/lib.hpp>
#include <crv/math/division/concepts.hpp>
#include <crv/math/division/result.hpp>
#include <crv/math/int_traits.hpp>
#include <crv/math/integer.hpp>
#include <climits>

namespace crv::division {

/// dispatches directly to hardware divider when possible, falls back to long divider when necessary
template <unsigned_integral dividend_t, unsigned_integral divisor_t,
          is_divider<dividend_t, divisor_t> hardware_divider_t, is_divider<dividend_t, divisor_t> long_divider_t>
struct dispatcher_t;

/// generic implementation forwards to hardware divider
template <unsigned_integral dividend_t, unsigned_integral divisor_t,
          is_divider<dividend_t, divisor_t> hardware_divider_t, is_divider<dividend_t, divisor_t> long_divider_t>
struct dispatcher_t
{
    auto operator()(dividend_t dividend, divisor_t divisor, hardware_divider_t hardware_divider,
                    long_divider_t) const noexcept -> auto
    {
        return hardware_divider(dividend, divisor);
    }
};

/**
    dispatches double-width dividends to long divider when quotient does not fit in destination type

    The hardware divider specializes double-width dividends. These specializations may trap if the quotient does not
    fit into the destination type. The long divider handles these correctly, so this specialization dispatches to the
    hardware divider when possible and the long divider when necessary.
*/
template <unsigned_integral divisor_t, is_divider<wider_t<divisor_t>, divisor_t> hardware_divider_t,
          is_divider<wider_t<divisor_t>, divisor_t> long_divider_t>
struct dispatcher_t<wider_t<divisor_t>, divisor_t, hardware_divider_t, long_divider_t>
{
    using dividend_t = wider_t<divisor_t>;

    auto operator()(dividend_t dividend, divisor_t divisor, hardware_divider_t hardware_divider,
                    long_divider_t long_divider) const noexcept -> result_t<dividend_t, divisor_t>
    {
        constexpr auto const single_width = sizeof(divisor) * CHAR_BIT;

        auto const quotient_fits = (dividend >> single_width) < int_cast<dividend_t>(divisor);
        if (quotient_fits)
        {
            // hardware result has single-width quotient
            auto const hardware_result = hardware_divider(dividend, divisor);

            // promote quotient to double-width to match long divider
            return {.quotient  = int_cast<dividend_t>(hardware_result.quotient),
                    .remainder = int_cast<divisor_t>(hardware_result.remainder)};
        }

        // long divider has double-width quotient
        return long_divider(dividend, divisor);
    }
};

} // namespace crv::division
