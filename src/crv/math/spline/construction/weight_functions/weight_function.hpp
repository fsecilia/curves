// SPDX-License-Identifier: MIT

/// \file
/// \brief perceptual domain error weights
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <cmath>

namespace crv::spline::weight_functions {

/// neutral weighting, treats all domain coordinates equally
template <typename scalar_t> struct uniform_t
{
    constexpr auto operator()(scalar_t) const noexcept -> scalar_t { return 1.0; }
};

/// weights error based on exponential decay using ae^-bt + c
template <typename scalar_t> struct exponential_decay_t
{
    scalar_t amplitude;
    scalar_t decay_rate;
    scalar_t baseline_offset;

    /// weights error using initial ratio and halflife
    ///
    /// \param ratio weight multiplier at t=0 relative to the tail (e.g., 10 for 10x priority)
    /// \param halflife distance along the curve, t, where the ratio is halved
    constexpr static auto from_ratio_and_halflife(scalar_t ratio, scalar_t halflife) noexcept -> exponential_decay_t
    {
        using std::log;
        return {.amplitude = ratio - 1, .decay_rate = log(2) / halflife, .baseline_offset = 1.0};
    }

    /// scales the given error magnitude based on its position along the curve
    constexpr auto operator()(scalar_t node) const noexcept -> scalar_t
    {
        using std::exp;
        return amplitude * exp(-decay_rate * node) + baseline_offset;
    }
};

/// weights error based on hyperbolic decay using 1/(t + a)
template <typename scalar_t> struct hyperbolic_decay_t
{
    scalar_t halflife;

    constexpr auto operator()(scalar_t node) const noexcept -> scalar_t { return 1.0 / (node + halflife); }
};

} // namespace crv::spline::weight_functions
