// SPDX-License-Identifier: MIT

/// \file
/// \brief quadrature rules for integrating over a range
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <numeric>

namespace crv::quadrature::rules {

/// evaluates a definite integral using 5-point Gauss-Legendre quadrature
struct gauss5_t
{
    template <typename real_t, typename func_t>
    constexpr auto operator()(real_t left, real_t right, func_t&& func) const noexcept -> real_t
    {
        static constexpr real_t abscissas[] = {0.0, 0.538469310105683, 0.906179845938664};
        static constexpr real_t weights[]   = {0.568888888888889, 0.478628670499366, 0.236926885056189};

        auto const midpoint   = std::midpoint(left, right);
        auto const half_width = (right - left) / real_t{2};

        auto sum = weights[0] * func(midpoint);
        for (auto i = 1; i < 3; ++i)
        {
            auto const offset = abscissas[i] * half_width;
            sum += weights[i] * (func(midpoint + offset) + func(midpoint - offset));
        }

        return sum * half_width;
    }
};
} // namespace crv::quadrature::rules
