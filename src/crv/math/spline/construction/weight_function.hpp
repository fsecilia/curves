// SPDX-License-Identifier: MIT

/// \file
/// \brief perceptual domain error weights
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <cmath>

namespace crv::weight_functions {

/// neutral weighting, treats all domain coordinates equally
template <typename real_t> struct uniform_t
{
    constexpr auto operator()(real_t) const noexcept -> real_t { return 1.0; }
};

/// weights error based on exponential decay using ae^-bt + c
template <typename real_t> struct exponential_decay_t
{
    real_t amplitude;
    real_t decay_rate;
    real_t baseline_offset;

    /// weights error using initial ratio and halflife
    ///
    /// \param ratio weight multiplier at t=0 relative to the tail (e.g., 10 for 10x priority)
    /// \param halflife distance along the curve, t, where the ratio is halved
    static auto from_ratio_and_halflife(real_t ratio, real_t halflife) noexcept -> exponential_decay_t
    {
        using std::log;

        return {ratio - 1, log(2) / halflife, 1.0};
    }

    /// scales the given error magnitude based on its position along the curve
    auto operator()(real_t magnitude, real_t target_position) const noexcept -> real_t
    {
        using std::exp;

        auto const weight = amplitude * exp(-decay_rate * target_position) + baseline_offset;
        return weight * magnitude;
    }
};

} // namespace crv::weight_functions
