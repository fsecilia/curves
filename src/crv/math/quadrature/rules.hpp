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

template <std::floating_point real_t> struct rule_result_t
{
    real_t value;
    real_t error;
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
            static_cast<real_t>(0.207784955007898467600689403773244913L),
            static_cast<real_t>(0.405845151377397166906606412076961463L),
            static_cast<real_t>(0.586087235467691130294144838258729598L),
            static_cast<real_t>(0.741531185599394439863864773280788407L),
            static_cast<real_t>(0.864864423359769072789712788640926201L),
            static_cast<real_t>(0.949107912342758524526189684047851262L),
            static_cast<real_t>(0.991455371120812639206854697526328517L),
        };

        // weights for the 15-point Kronrod rule
        static constexpr auto   kronrod_center_weight = static_cast<real_t>(0.209482141084727828012999174891714264L);
        static constexpr real_t kronrod_weights[symmetric_sample_count] = {
            static_cast<real_t>(0.204432940075298892414161999234649085L),
            static_cast<real_t>(0.190350578064785409913256402421013683L),
            static_cast<real_t>(0.169004726639267902826583426598550284L),
            static_cast<real_t>(0.140653259715525918745189590510237920L),
            static_cast<real_t>(0.104790010322250183839876322541518017L),
            static_cast<real_t>(0.0630920926299785532907006631892042867L),
            static_cast<real_t>(0.0229353220105292249637320080589695920L),
        };

        // weights for the embedded 7-point Gauss rule, padded with zeros
        static constexpr real_t gauss_center_weight = static_cast<real_t>(0.417959183673469387755102040816326531L);
        static constexpr real_t gauss_weights[symmetric_sample_count] = {
            static_cast<real_t>(0.000000000000000000000000000000000000L),
            static_cast<real_t>(0.381830050505118944950369775488975134L),
            static_cast<real_t>(0.000000000000000000000000000000000000L),
            static_cast<real_t>(0.279705391489276667901467771423779582L),
            static_cast<real_t>(0.000000000000000000000000000000000000L),
            static_cast<real_t>(0.129484966168869693270611432679082018L),
            static_cast<real_t>(0.000000000000000000000000000000000000L),
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
