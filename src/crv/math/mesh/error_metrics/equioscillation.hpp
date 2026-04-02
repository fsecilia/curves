// SPDX-License-Identifier: MIT

/// \file
/// \brief error metric that samples at equioscillation extrema
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/mesh/measured_error.hpp>
#include <array>
#include <cmath>
#include <numbers>

namespace crv::equioscillation {

// --------------------------------------------------------------------------------------------------------------------
// Node Cache
// --------------------------------------------------------------------------------------------------------------------

/// caches precalculated equioscillation extrema at Chebyshev nodes of the second kind
template <typename real_t, int node_count = 15> struct node_cache_t
{
    using nodes_t = std::array<real_t, node_count>;
    static nodes_t const nodes;
};

template <typename real_t, int node_count>
node_cache_t<real_t, node_count>::nodes_t const node_cache_t<real_t, node_count>::nodes = []() {
    static_assert(node_count > 1, "must have at least 2 nodes to form an interval");

    nodes_t result;

    static constexpr auto scale = std::numbers::pi_v<real_t> / static_cast<real_t>(node_count - 1);
    for (auto node = 0; node < node_count; ++node) { result[node] = std::cos(static_cast<real_t>(node) * scale); }

    return result;
}();

// --------------------------------------------------------------------------------------------------------------------
// Error Metric
// --------------------------------------------------------------------------------------------------------------------

/// error metric approximating L-infinity norm at equioscillation extrema
///
/// This type evaluates the error between the generated payload and the ideal function at natural equioscillation
/// points, capturing the maximum error across the interval.
template <typename real_t, typename node_cache_t, typename ideal_function_t, typename evaluator_t> struct error_metric_t
{
    ideal_function_t ideal_function;
    evaluator_t      evaluator;

    auto operator()(auto const& payload, real_t left, real_t right) const noexcept -> measured_error_t<real_t>
    {
        auto const center = (right + left) * 0.5;
        auto const radius = (right - left) * 0.5;

        auto result = measured_error_t<real_t>{.position = center, .magnitude = 0.0};
        for (auto node : node_cache_t::nodes)
        {
            auto const position      = center + radius * node;
            auto const ideal         = ideal_function(position);
            auto const approximation = evaluator(payload, position);
            auto const magnitude     = std::abs(ideal - approximation);

            if (magnitude > result.magnitude)
            {
                result.position  = position;
                result.magnitude = magnitude;
            }
        }

        return result;
    }
};

} // namespace crv::equioscillation
