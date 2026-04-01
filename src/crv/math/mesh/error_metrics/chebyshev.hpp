// SPDX-License-Identifier: MIT

/// \file
/// \brief error metric using Chebyshev nodes
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <array>
#include <cmath>
#include <numbers>

namespace crv::chebyshev {

// --------------------------------------------------------------------------------------------------------------------
// Node Cache
// --------------------------------------------------------------------------------------------------------------------

// caches precalculated Chebyshev node positions
template <typename real_t, int node_count = 15> struct node_cache_t
{
    using nodes_t = std::array<real_t, node_count>;
    static nodes_t const nodes;
};

template <typename real_t, int node_count>
node_cache_t<real_t, node_count>::nodes_t const node_cache_t<real_t, node_count>::nodes = []() {
    nodes_t result;

    static constexpr auto scale = std::numbers::pi_v<real_t> / static_cast<real_t>(2 * node_count);
    for (auto node = 1; node <= node_count; ++node)
    {
        result[node - 1] = std::cos(static_cast<real_t>(2 * node - 1) * scale);
    }

    return result;
}();

} // namespace crv::chebyshev
