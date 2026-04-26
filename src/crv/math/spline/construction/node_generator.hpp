// SPDX-License-Identifier: MIT

/// \file
/// \brief policies to choose where to sample a curve
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <array>
#include <cmath>
#include <numbers>

namespace crv::node_generators {

/// generates equioscillation extrema at Chebyshev nodes of the second kind
template <typename real_t, int sample_count = 5> struct equioscillation_t
{
    using nodes_t = std::array<real_t, sample_count>;
    static nodes_t const nodes;

    auto operator()() const noexcept -> nodes_t const& { return nodes; }
};

template <typename real_t, int sample_count>
equioscillation_t<real_t, sample_count>::nodes_t const equioscillation_t<real_t, sample_count>::nodes
    = []() noexcept -> nodes_t {
    static_assert(sample_count > 1, "must have at least 2 nodes to form an interval");

    nodes_t result;

    // calc mid-range values
    static constexpr auto scale = std::numbers::pi_v<real_t> / static_cast<real_t>(sample_count - 1);
    for (auto sample = 1; sample < sample_count - 1; ++sample)
    {
        auto const position = std::cos(static_cast<real_t>(sample) * scale);
        result[sample] = -position;
    }

    // punch in common values
    result[0] = -1.0;
    result[sample_count - 1] = 1.0;

    constexpr auto odd_count = sample_count & 1;
    if constexpr (odd_count) result[sample_count / 2] = 0.0;

    return result;
}();

} // namespace crv::node_generators
