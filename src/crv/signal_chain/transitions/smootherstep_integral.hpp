// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/scalar_traits.hpp>

namespace crv::signal_chain::transitions {

// transition based on integral of smootherstep
struct smootherstep_integral_t
{
    // returns normalized integral of smootherstep ranging over (t, y, y') in [(0, 0, 0), (1, 0.5, 1)]
    template <typename value_t> constexpr auto operator()(value_t input) const noexcept -> value_t
    {
        using scalar_t = scalar_type_t<value_t>;

        auto const t = primal(input);
        if (t <= scalar_t{0}) return value_t{0};
        if (t >= scalar_t{1}) return input - scalar_t{0.5};

        // strictly compute using the primal scalar to avoid implicit jet evaluation
        auto const t2 = t * t;
        auto const t4 = t2 * t2;

        // f = t^6 - 3t^5 + 2.5t^4
        auto const y = t4 * (t2 - scalar_t{3} * t + scalar_t{2.5});

        if constexpr (is_jet<value_t>)
        {
            // df = 6t^5 - 15t^4 + 10t^3
            auto const t3 = t2 * t;
            auto const dt = tangent(input);
            auto const dy_dt = t3 * ((scalar_t{6} * t - scalar_t{15}) * t + scalar_t{10});
            return value_t{y, dy_dt * dt};
        }
        else return y;
    }
};

} // namespace crv::signal_chain::transitions
