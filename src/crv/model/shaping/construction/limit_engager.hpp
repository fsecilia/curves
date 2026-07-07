// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <cassert>
#include <concepts>

namespace crv::shaping::construction {

/// linearly engages limiter when the limit is above, but near the curve
///
/// This type rolls the limit on gradually as the limit approaches the top of the curve. Without this, the curve would
/// jump from 0% engagement to 100% engagement and appear to pop.
template <std::floating_point real_t> struct limiter_engager_t
{
    /// \param output_max maximum output of curve
    /// \param output_limit configured output limit
    /// \param output_height vertical distance below limit where engagement begins to roll on
    /// \pre transition_height > 0
    [[nodiscard]] constexpr auto operator()(
        real_t output_max, real_t output_limit, real_t transition_height) const noexcept -> real_t
    {
        assert(transition_height > real_t{0});

        if (output_limit <= output_max) return real_t{1};

        auto const output_remaining = output_limit - output_max;
        if (transition_height <= output_remaining) return real_t{0};

        return real_t{1} - output_remaining / transition_height;
    }
};

} // namespace crv::shaping::construction
