// SPDX-License-Identifier: MIT

/// \file
/// \brief floating point cubic hermite segment
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <array>
#include <cassert>
#include <cmath>

namespace crv::spline::floating_point {

template <std::floating_point real_t> struct segment_t
{
    static constexpr auto coeff_count = 4;

    std::array<real_t, coeff_count> coeffs;

    [[nodiscard]] auto evaluate(real_t t) const noexcept -> real_t
    {
        auto result = coeffs[0];
        for (auto coeff = 1; coeff < coeff_count; ++coeff) result = std::fma(result, t, coeffs[coeff]);
        return result;
    }
};

} // namespace crv::spline::floating_point
