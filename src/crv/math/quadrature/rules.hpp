// SPDX-License-Identifier: MIT

/// \file
/// \brief quadrature rules for integrating over a range
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/abs.hpp>
#include <concepts>
#include <numeric>

namespace crv::quadrature::rules {

template <typename t_real_t, int_t sample_count> struct gauss_kronrod_t;

/// definite integral using 15-point Gauss-Kronrod quadrature (G7/K15)
template <typename t_real_t> struct gauss_kronrod_t<t_real_t, 15>
{
    using real_t = t_real_t;

    struct estimate_t
    {
        /// integral sum
        real_t sum;

        /// internal error estimate for value; must be non-negative
        real_t error;

        constexpr auto operator<=>(estimate_t const&) const noexcept -> auto = default;
        constexpr auto operator==(estimate_t const&) const noexcept -> bool  = default;
    };

    // K15 has 1 center point and 7 symmetric pairs
    static constexpr auto k15_symmetric_sample_count = 7;

    // positive abscissas; G7 uses the odd indices, k15 uses them all
    static constexpr real_t abscissas[k15_symmetric_sample_count] = {
        static_cast<real_t>(0x1.a98b2892e0c768600a87986cb9abp-3Q),
        static_cast<real_t>(0x1.9f95df119fd61b073a662ad1b710p-2Q),
        static_cast<real_t>(0x1.2c13a049dfa23d7b9520011f0c69p-1Q),
        static_cast<real_t>(0x1.7ba9f9be3a1d5d160239fdb75338p-1Q),
        static_cast<real_t>(0x1.bacf827b9bb3dc8eb243a23a7db2p-1Q),
        static_cast<real_t>(0x1.e5f178e7c622958378458368364dp-1Q),
        static_cast<real_t>(0x1.fba009d4d09b13f001a1ae4bce02p-1Q),
    };

    // weights for K15 rule
    static constexpr auto   k15_center_weight = static_cast<real_t>(0x1.ad04f9087090f55f92f946c81e39p-3Q);
    static constexpr real_t k15_weights[k15_symmetric_sample_count] = {
        static_cast<real_t>(0x1.a2adbcbec9cd83e2b52e31473ed2p-3Q),
        static_cast<real_t>(0x1.85d6861c80eb0a74da6666816adcp-3Q),
        static_cast<real_t>(0x1.5a1f266e47d5bba3643f24a3e1dep-3Q),
        static_cast<real_t>(0x1.200ed0f46e8c0fdb57173cf54406p-3Q),
        static_cast<real_t>(0x1.ad384a34814c5b7efab57bd3e8d4p-4Q),
        static_cast<real_t>(0x1.026cdaa7b61c3ac5094c5937c40ap-4Q),
        static_cast<real_t>(0x1.77c5b67d574702bf4cbbc5a25713p-6Q),
    };

    // G7 has 1 center and 3 symmetric pairs
    static constexpr auto g7_symmetric_sample_count = 3;

    // weights for embedded G7 rule
    static constexpr real_t g7_center_weight = static_cast<real_t>(0x1.abfd7e03c2fa5b8876b34df30b13p-2Q);
    static constexpr real_t g7_weights[g7_symmetric_sample_count] = {
        static_cast<real_t>(0x1.86fe74ee32b3d64d2fa1cfb4c0c5p-2Q),
        static_cast<real_t>(0x1.1e6b1713d86446b4d09badbb8787p-2Q),
        static_cast<real_t>(0x1.092f69f826d56a7388d1b72c6454p-3Q),
    };

    template <std::invocable<real_t> integrand_t>
    constexpr auto estimate(real_t left, real_t right, integrand_t const& integrand) const noexcept -> estimate_t
    {
        auto const midpoint   = std::midpoint(left, right);
        auto const half_width = (right - left) / real_t{2};

        // evaluate center point
        auto const center_val = integrand(midpoint);
        auto       k15_sum    = k15_center_weight * center_val;
        auto       g7_sum     = g7_center_weight * center_val;

        // evaluate symmetric pairs in chunks of two
        for (auto i = 0; i < g7_symmetric_sample_count; ++i)
        {
            auto const even_idx = i * 2;
            auto const odd_idx  = even_idx + 1;

            // even index (K15 only)
            auto const offset_even        = abscissas[even_idx] * half_width;
            auto const symmetric_sum_even = integrand(midpoint + offset_even) + integrand(midpoint - offset_even);

            k15_sum += k15_weights[even_idx] * symmetric_sum_even;

            // odd index (K15 and G7)
            auto const offset_odd        = abscissas[odd_idx] * half_width;
            auto const symmetric_sum_odd = integrand(midpoint + offset_odd) + integrand(midpoint - offset_odd);

            k15_sum += k15_weights[odd_idx] * symmetric_sum_odd;
            g7_sum += g7_weights[i] * symmetric_sum_odd;
        }

        // Handle the final even K15 pair
        static constexpr auto last_idx = k15_symmetric_sample_count - 1;

        auto const offset_last = abscissas[last_idx] * half_width;
        auto const sum_last    = integrand(midpoint + offset_last) + integrand(midpoint - offset_last);

        k15_sum += k15_weights[last_idx] * sum_last;

        // error is the magnitude of the difference between the two rules
        using crv::abs;
        auto const error = abs((k15_sum - g7_sum) * half_width);

        auto const sum = k15_sum * half_width;

        return estimate_t{sum, error};
    }

    template <std::invocable<real_t> integrand_t>
    constexpr auto integrate(real_t left, real_t right, integrand_t const& integrand) const noexcept -> real_t
    {
        auto const midpoint   = std::midpoint(left, right);
        auto const half_width = (right - left) / real_t{2};

        // evaluate center point
        auto const center_val = integrand(midpoint);
        auto       k15_sum    = k15_center_weight * center_val;

        // evaluate symmetric pairs
        for (auto i = 0; i < k15_symmetric_sample_count; ++i)
        {
            auto const offset        = abscissas[i] * half_width;
            auto const symmetric_sum = integrand(midpoint + offset) + integrand(midpoint - offset);

            k15_sum += k15_weights[i] * symmetric_sum;
        }

        auto const sum = k15_sum * half_width;

        return sum;
    }

    constexpr auto operator<=>(gauss_kronrod_t const&) const noexcept -> auto = default;
    constexpr auto operator==(gauss_kronrod_t const&) const noexcept -> bool  = default;
};

} // namespace crv::quadrature::rules
