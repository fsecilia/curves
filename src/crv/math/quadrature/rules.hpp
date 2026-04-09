// SPDX-License-Identifier: MIT

/// \file
/// \brief quadrature rules for integrating over a range
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/abs.hpp>
#include <concepts>
#include <numeric>

namespace crv::quadrature {

/// common result of quadrature rules
template <std::floating_point real_t> struct rule_result_t
{
    /// integral sum
    real_t value;

    /// internal error estimate for value; must be non-negative
    real_t error;

    auto operator<=>(rule_result_t const&) const noexcept -> auto = default;
    auto operator==(rule_result_t const&) const noexcept -> bool  = default;
};

namespace rules {

template <int sample_count> struct gauss_kronrod_t;

/// evaluates a definite integral using 15-point Gauss-Kronrod quadrature (G7/K15)
template <> struct gauss_kronrod_t<15>
{
    template <std::floating_point real_t, std::invocable<real_t> integrand_t>
    constexpr auto operator()(real_t left, real_t right, integrand_t const& integrand) const noexcept
        -> rule_result_t<real_t>
    {
        // K15 has 1 center point and 7 symmetric pairs
        static constexpr auto symmetric_sample_count = 7;

        // positive abscissas for the 15-point Kronrod rule
        static constexpr real_t abscissas[symmetric_sample_count] = {
            static_cast<real_t>(0x1.a98b2892e0c768600a87986cb9abp-3Q),
            static_cast<real_t>(0x1.9f95df119fd61b073a662ad1b710p-2Q),
            static_cast<real_t>(0x1.2c13a049dfa23d7b9520011f0c69p-1Q),
            static_cast<real_t>(0x1.7ba9f9be3a1d5d160239fdb75338p-1Q),
            static_cast<real_t>(0x1.bacf827b9bb3dc8eb243a23a7db2p-1Q),
            static_cast<real_t>(0x1.e5f178e7c622958378458368364dp-1Q),
            static_cast<real_t>(0x1.fba009d4d09b13f001a1ae4bce02p-1Q),
        };

        // weights for the 15-point Kronrod rule
        static constexpr auto   kronrod_center_weight = static_cast<real_t>(0x1.ad04f9087090f55f92f946c81e39p-3Q);
        static constexpr real_t kronrod_weights[symmetric_sample_count] = {
            static_cast<real_t>(0x1.a2adbcbec9cd83e2b52e31473ed2p-3Q),
            static_cast<real_t>(0x1.85d6861c80eb0a74da6666816adcp-3Q),
            static_cast<real_t>(0x1.5a1f266e47d5bba3643f24a3e1dep-3Q),
            static_cast<real_t>(0x1.200ed0f46e8c0fdb57173cf54406p-3Q),
            static_cast<real_t>(0x1.ad384a34814c5b7efab57bd3e8d4p-4Q),
            static_cast<real_t>(0x1.026cdaa7b61c3ac5094c5937c40ap-4Q),
            static_cast<real_t>(0x1.77c5b67d574702bf4cbbc5a25713p-6Q),
        };

        // weights for the embedded 7-point Gauss rule, padded with zeros
        static constexpr real_t gauss_center_weight = static_cast<real_t>(0x1.abfd7e03c2fa5b8876b34df30b13p-2Q);
        static constexpr real_t gauss_weights[symmetric_sample_count] = {
            static_cast<real_t>(0x0.0000000000000000000000000000p+0Q),
            static_cast<real_t>(0x1.86fe74ee32b3d64d2fa1cfb4c0c5p-2Q),
            static_cast<real_t>(0x0.0000000000000000000000000000p+0Q),
            static_cast<real_t>(0x1.1e6b1713d86446b4d09badbb8787p-2Q),
            static_cast<real_t>(0x0.0000000000000000000000000000p+0Q),
            static_cast<real_t>(0x1.092f69f826d56a7388d1b72c6454p-3Q),
            static_cast<real_t>(0x0.0000000000000000000000000000p+0Q),
        };

        auto const midpoint   = std::midpoint(left, right);
        auto const half_width = (right - left) / real_t{2};

        // evaluate center point (offset = 0)
        auto const center_val  = integrand(midpoint);
        auto       kronrod_sum = kronrod_center_weight * center_val;
        auto       gauss_sum   = gauss_center_weight * center_val;

        // evaluate symmetric pairs
        for (auto i = 0; i < symmetric_sample_count; ++i)
        {
            auto const offset = abscissas[i] * half_width;
            auto const f_sum  = integrand(midpoint + offset) + integrand(midpoint - offset);

            kronrod_sum += kronrod_weights[i] * f_sum;
            gauss_sum += gauss_weights[i] * f_sum;
        }

        auto const integral = kronrod_sum * half_width;

        // error is the magnitude of the difference between the two rules
        using crv::abs;
        auto const error = abs((kronrod_sum - gauss_sum) * half_width);

        return {integral, error};
    }
};

} // namespace rules
} // namespace crv::quadrature
