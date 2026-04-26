// SPDX-License-Identifier: MIT

/// \file
/// \brief residual estimator for adaptive mesh refinement
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <algorithm>
#include <numeric>

namespace crv {

/// \brief estimates the maximum residual error of an approximant over a specific domain interval
///
/// This type is an orchestrator. It organizes a search to find the worst-case error between a target function and its
/// generated approximant. The search space is defined by a node generator to find collocation points, the domain is
/// quantized to fixed-point precision prior to evaluation, the residual is calculated using an error norm, and the
/// magnitude of the residual is scaled by perceptual significance using a weight function.
template <typename real_t, typename target_function_t, typename approximant_evaluator_t, typename node_generator_t,
    typename quantizer_t, typename error_norm_t, typename weight_function_t>
struct residual_estimator_t
{
    target_function_t evaluate_target;
    approximant_evaluator_t evaluate_approximant;
    node_generator_t generate_nodes;
    quantizer_t quantize;
    error_norm_t measure_error;
    weight_function_t apply_weight;

    auto operator()(auto const& approximant, real_t left, real_t right) const noexcept -> real_t
    {
        auto const midpoint = std::midpoint(right, left);
        auto const half_width = (right - left) * 0.5;

        auto max_magnitude = real_t{0};

        for (auto const standard_node : generate_nodes())
        {
            // convert from standard nodes in [-1, 1] to domain nodes in [left, right].
            //
            // Here, we sub exact center and boundries with no truncation error using cmov. Applying this to left and
            // right prevents truncation from landing the evaluated position outside of the domain. We also apply it to
            // center because it's trivial to and center tends to suffer from truncation.
            auto domain_node = midpoint + half_width * standard_node;
            if (standard_node == -1.0) domain_node = left;
            if (standard_node == 0.0) domain_node = midpoint;
            if (standard_node == 1.0) domain_node = right;

            // evaluate target at target position and approxmant at quantized position
            auto const quantized_node = quantize(domain_node);
            auto const target = evaluate_target(domain_node);
            auto const approximation = evaluate_approximant(approximant, quantized_node);

            // measure magnitude of error using norm, weight it using weight function
            auto const magnitude = measure_error(target, approximation);
            auto const weighted_magnitude = magnitude * apply_weight(domain_node);
            max_magnitude = std::max(max_magnitude, weighted_magnitude);
        }

        return max_magnitude;
    }
};

} // namespace crv
