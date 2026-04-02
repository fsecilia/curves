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

    // calc mid-range values
    static constexpr auto scale = std::numbers::pi_v<real_t> / static_cast<real_t>(node_count - 1);
    for (auto node = 1; node < node_count - 1; ++node)
    {
        auto const position = std::cos(static_cast<real_t>(node) * scale);
        result[node]        = -position;
    }

    // punch in common values
    result[0]              = -1.0;
    result[node_count / 2] = 0.0;
    result[node_count - 1] = 1.0;

    return result;
}();

} // namespace crv::equioscillation
