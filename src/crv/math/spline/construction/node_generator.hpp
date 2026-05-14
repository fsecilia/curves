// SPDX-License-Identifier: MIT

/// \file
/// \brief policies to choose where to sample a curve
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <array>
#include <cmath>
#include <numbers>

namespace crv::spline::node_generators {

// --------------------------------------------------------------------------------------------------------------------
// equisocillation
// --------------------------------------------------------------------------------------------------------------------

/// generates equioscillation extrema at Chebyshev nodes of the second kind in [0, 1], excluding the endpoints
template <typename scalar_t, int sample_count = 3> struct equioscillation_t
{
    using nodes_t = std::array<scalar_t, sample_count>;
    static nodes_t const nodes;

    auto operator()() const noexcept -> nodes_t const& { return nodes; }
};

template <typename scalar_t, int sample_count>
equioscillation_t<scalar_t, sample_count>::nodes_t const equioscillation_t<scalar_t, sample_count>::nodes
    = []() noexcept -> nodes_t {
    static_assert(sample_count > 1, "must have at least 2 nodes to form an interval");

    nodes_t result;

    // calc mid-range values
    static constexpr auto scale = std::numbers::pi_v<scalar_t> / static_cast<scalar_t>(sample_count + 1);
    for (auto sample = 0; sample < sample_count; ++sample)
    {
        auto const position = std::cos(static_cast<scalar_t>(sample + 1) * scale);
        result[sample] = (1 - position) * 0.5;
    }

    return result;
}();

} // namespace crv::spline::node_generators
