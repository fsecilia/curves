// SPDX-License-Identifier: MIT

/// \file
/// \brief quantiziation policies
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <cmath>

namespace crv {

/// simulates fixed-point precision loss
template <typename scalar_t, int frac_bits> struct fixed_quantizer_t
{
    /// \returns rint(value*2^frac_bits)/2^frac_bits, optimized by ldexp
    static auto operator()(scalar_t value) noexcept -> scalar_t
    {
        using std::ldexp;
        using std::rint;

        // scale up by directly manipulating the exponent
        auto const scaled = ldexp(value, frac_bits);

        // round and scale back down
        return ldexp(rint(scaled), -frac_bits);
    }
};

} // namespace crv
