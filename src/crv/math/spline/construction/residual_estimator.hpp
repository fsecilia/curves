// SPDX-License-Identifier: MIT

/// \file
/// \brief residual estimator for adaptive mesh refinement
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/abs.hpp>
#include <cassert>
#include <cmath>

namespace crv::spline {

/// max scale of target and error between target and approximant over a subdomain
template <std::floating_point real_t> struct residual_t
{
    real_t metric_error; // error based on norm error metric
    real_t weighted_error; // metric_error weighted perceptually
    real_t scale; // absolute magnitude of primal

    constexpr auto operator==(residual_t const&) const noexcept -> bool = default;
};

/// estimates the maximum residual error of an approximant over a specific domain interval
///
/// This searches to find the worst-case error between a target function and its approximant. The search space is
/// defined by a node generator to find collocation points, the domain is evaluated in fixed-point, the residual is
/// calculated using an error norm, and the magnitude of the residual is scaled by perceptual significance using a
/// weight function.
template <std::floating_point real_t, typename node_generator_t, typename error_norm_t, typename weight_function_t>
struct residual_estimator_t
{
    using residual_t = residual_t<real_t>;

    [[no_unique_address]] node_generator_t generate_nodes;
    error_norm_t measure_error;
    weight_function_t apply_weight;

    constexpr auto operator()(auto const& sample_target_function, auto const& approximant, real_t left,
        real_t right) const noexcept -> residual_t
    {
        using std::isfinite;

        auto max_residual = residual_t{};

        auto const interval_width = right - left;

        // sample function at generated nodes
        for (auto const standard_node : generate_nodes())
        {
            // convert from standard nodes in [0, 1] to domain nodes in [left, right].
            auto const domain_node = left + standard_node * interval_width;

            // evaluate target function and approximant
            auto const approximation = approximant(domain_node);
            auto const target_function_sample = sample_target_function(domain_node);

            auto const target = target_function_sample.y;
            auto const metric_error = measure_error(target, approximation);
            assert(isfinite(metric_error));

            max_residual.scale = max(max_residual.scale, abs(primal(target)));
            max_residual.metric_error = max(max_residual.metric_error, abs(metric_error));
        }

        auto const midpoint = (left + right) * 0.5;
        auto const weight = apply_weight(midpoint);
        assert(isfinite(weight));
        max_residual.weighted_error = max_residual.metric_error * weight;
        return max_residual;
    }
};

} // namespace crv::spline
