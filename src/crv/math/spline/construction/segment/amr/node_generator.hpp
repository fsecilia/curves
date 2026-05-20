// SPDX-License-Identifier: MIT

/// \file
/// \brief chooses where to sample a curve
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <array>
#include <cmath>
#include <numbers>

namespace crv::spline {

/// generates equioscillation extrema at Chebyshev nodes of the second kind in (0, 1), excluding the endpoints
///
/// The endpoints are excluded because these cubics start in hermite form, which means they go through the knots
/// by construction at the endpoints, so these will always have error of effectively zero.
template <typename scalar_t, int_t sample_count> struct node_generator_t
{
    using nodes_t = std::array<scalar_t, sample_count>;
    static nodes_t const nodes;

    auto operator()() const noexcept -> nodes_t const& { return nodes; }
};

template <typename scalar_t, int_t sample_count>
node_generator_t<scalar_t, sample_count>::nodes_t const node_generator_t<scalar_t, sample_count>::nodes
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

} // namespace crv::spline
