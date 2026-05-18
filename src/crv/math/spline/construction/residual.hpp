// SPDX-License-Identifier: MIT

/// \file
/// \brief tools for measuring residual error
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/abs.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/polynomial.hpp>
#include <cassert>
#include <cmath>

namespace crv::spline {

/// max scale of target and error between target and approximant over a subdomain
template <std::floating_point scalar_t> struct residual_t
{
    scalar_t metric_error; // error based on norm error metric
    scalar_t weighted_error; // metric_error weighted perceptually
    scalar_t scale; // absolute magnitude of primal

    constexpr auto operator==(residual_t const&) const noexcept -> bool = default;
};

/// estimates the maximum residual error of an approximant over a specific domain interval
///
/// This type searches to find the worst-case error between a target function and its approximant. The search space is
/// defined by a node generator to find collocation points, the domain is evaluated in fixed-point, the residual is
/// calculated using an error norm, and the magnitude of the residual is scaled by perceptual significance using a
/// weight function.
template <std::floating_point scalar_t, typename node_generator_t, typename error_norm_t, typename weight_function_t>
struct residual_estimator_t
{
    using residual_t = residual_t<scalar_t>;

    [[no_unique_address]] node_generator_t generate_nodes;
    error_norm_t measure_error;
    weight_function_t apply_weight;

    constexpr auto operator()(auto const& sample_target_function, auto const& approximant, scalar_t left,
        scalar_t midpoint, scalar_t right) const noexcept -> residual_t
    {
        auto const interval_width = right - left;

        // sample function at generated nodes, calc error, and track extrema
        auto max_residual = residual_t{};
        for (auto const standard_node : generate_nodes())
        {
            // convert from standard nodes in [0, 1] to domain nodes in [left, right].
            auto const domain_node = left + standard_node * interval_width;

            // measure error between target function and approximant
            auto const approximation = approximant(domain_node);
            auto const target = sample_target_function(domain_node).y;
            auto const metric_error = measure_error(target, approximation);

            // track extrema
            max_residual.scale = max(max_residual.scale, abs(target));
            max_residual.metric_error = max(max_residual.metric_error, abs(metric_error));
        }

        auto const weight = apply_weight(midpoint);
        max_residual.weighted_error = max_residual.metric_error * weight;
        return max_residual;
    }
};

/// L-infinity norm for scalars used to measure error in position
struct absolute_error_norm_t
{
    template <typename scalar_t>
    static constexpr auto operator()(scalar_t target, scalar_t approximation) noexcept -> scalar_t
    {
        using std::isfinite;

        auto const result = abs(target - approximation);
        assert(isfinite(result));

        return result;
    }
};

/// approximates a segment with a cubic polynomial
template <std::floating_point t_scalar_t, is_fixed t_x_t> struct approximant_t
{
    using scalar_t = t_scalar_t;
    using x_t = t_x_t;

    cubic_t<scalar_t> cubic;
    x_t x0;
    int_t log2_width;

    /// \returns spline-global spatial coordinate y
    constexpr auto operator()(scalar_t x) const noexcept -> scalar_t
    {
        auto const x_local = from_fixed<scalar_t>(to_fixed<x_t>(x) - x0);
        auto const t = std::ldexp(x_local, int_cast<int>(-log2_width));
        return cubic(t);
    }
};

} // namespace crv::spline
