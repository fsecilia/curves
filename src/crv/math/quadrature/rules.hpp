// SPDX-License-Identifier: MIT

/// \file
/// \brief quadrature rules for integrating over a range
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <concepts>
#include <numeric>

namespace crv::quadrature::rules {

/// evaluates a definite integral using Gauss-Legendre quadrature
template <int sample_count> struct gauss_t;

/// evaluates a definite integral using 4-point Gauss-Legendre quadrature
template <> struct gauss_t<4>
{
    template <std::floating_point real_t, std::invocable<real_t> integrand_t>
    constexpr auto operator()(real_t left, real_t right, integrand_t const& integrand) const noexcept -> real_t
    {
        static constexpr auto   symmetric_sample_count            = 2;
        static constexpr real_t abscissas[symmetric_sample_count] = {0.3399810435848563, 0.8611363115940526};
        static constexpr real_t weights[symmetric_sample_count]   = {0.6521451548625461, 0.3478548451374538};

        auto const midpoint   = std::midpoint(left, right);
        auto const half_width = (right - left) / real_t{2};

        real_t sum = real_t{0};
        for (auto i = 0; i < symmetric_sample_count; ++i)
        {
            auto const offset = abscissas[i] * half_width;
            sum += weights[i] * (integrand(midpoint + offset) + integrand(midpoint - offset));
        }

        return sum * half_width;
    }
};

/// evaluates a definite integral using 5-point Gauss-Legendre quadrature
template <> struct gauss_t<5>
{
    template <std::floating_point real_t, std::invocable<real_t> integrand_t>
    constexpr auto operator()(real_t left, real_t right, integrand_t const& integrand) const noexcept -> real_t
    {
        static constexpr auto   symmetric_sample_count            = 3;
        static constexpr real_t abscissas[symmetric_sample_count] = {0.0, 0.538469310105683, 0.906179845938664};
        static constexpr real_t weights[symmetric_sample_count]
            = {0.568888888888889, 0.478628670499366, 0.236926885056189};

        auto const midpoint   = std::midpoint(left, right);
        auto const half_width = (right - left) / real_t{2};

        auto sum = weights[0] * integrand(midpoint);
        for (auto i = 1; i < symmetric_sample_count; ++i)
        {
            auto const offset = abscissas[i] * half_width;
            sum += weights[i] * (integrand(midpoint + offset) + integrand(midpoint - offset));
        }

        return sum * half_width;
    }
};

} // namespace crv::quadrature::rules
