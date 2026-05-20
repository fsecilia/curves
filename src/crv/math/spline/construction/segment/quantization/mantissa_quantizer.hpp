// SPDX-License-Identifier: MIT

/// \file
/// \brief coefficient mantissa quantizer
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/shifter.hpp>
#include <climits>

namespace crv::spline {

/// applies right-shifts to coefficients, flushing to zero when the shift exceeds container size
template <signed_integral mantissa_t, auto shifter = shifter_t<>{}> struct mantissa_quantizer_t
{
    static constexpr auto max_container_shift = int_t{sizeof(mantissa_t) * CHAR_BIT} - 1;

    constexpr auto operator()(mantissa_t mantissa, int_t preshift) const noexcept -> mantissa_t
    {
        if (preshift >= max_container_shift) return 0;
        return shifter.shr(mantissa, preshift);
    }
};

} // namespace crv::spline
