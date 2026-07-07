// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/math/scalar_traits.hpp>

namespace crv::shaping::transitions {

/// quadratic transition
struct quadratic_t
{
    /// returns (x, y, y') in [(0,0,0), (1, 0.5, 1)], gives c1 continuity
    template <typename value_t> constexpr auto operator()(value_t input) const noexcept -> value_t
    {
        using scalar_t = scalar_type_t<value_t>;
        auto const t = primal(input);
        if (t <= scalar_t{0}) return value_t{0};
        if (t >= scalar_t{1}) return input - scalar_t{0.5};

        auto const y = t * t / scalar_t{2};
        if constexpr (is_jet<value_t>)
        {
            auto const dt = tangent(input);
            return value_t{y, t * dt};
        }
        else return y;
    }
};

} // namespace crv::shaping::transitions
