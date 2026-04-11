// SPDX-License-Identifier: MIT

/// \file
/// \brief floating point cubic hermite segment
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <array>
#include <cassert>

namespace crv::spline::floating_point {

template <typename real_t> struct segment_t
{
    static constexpr auto coeff_count = 4;

    std::array<real_t, coeff_count> coeffs;

    constexpr auto evaluate(real_t t) const noexcept -> real_t
    {
        assert(0.0 <= t && t <= 1.0 && "floating_point::segment_t: t must be in [0, 1]");

        auto result = coeffs[0];
        for (auto coeff = 1; coeff < coeff_count; ++coeff)
        {
            result *= t;
            result += coeffs[coeff];
        }
        return result;
    }
};

} // namespace crv::spline::floating_point
