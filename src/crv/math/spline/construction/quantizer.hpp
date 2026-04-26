// SPDX-License-Identifier: MIT

/// \file
/// \brief quantiziation policies
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <cmath>

namespace crv::quantizers {

/// simulates fixed-point precision loss
template <typename real_t, int frac_bits> struct fixed_point_t
{
    /// \returns rint(value*2^frac_bits)/2^frac_bits, optimized by ldexp
    static auto operator()(real_t value) noexcept -> real_t
    {
        using std::ldexp;
        using std::rint;

        // scale up by directly manipulating the exponent
        auto const scaled = ldexp(value, frac_bits);

        // round and scale back down
        return ldexp(rint(scaled), -frac_bits);
    }
};

/// passthrough policy when metrics need no quantization
struct no_op_t
{
    template <typename real_t> static constexpr auto operator()(real_t value) noexcept -> real_t { return value; }
};

} // namespace crv::quantizers
