// SPDX-License-Identifier: MIT

/// \file
/// \brief convergence test
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <algorithm>
#include <concepts>
#include <limits>

namespace crv::spline {

/// decides if an interval should subdivide
///
/// Compares the norm's metric_error directly against global_tolerance and a noise floor, both in y.
template <std::floating_point scalar_t, int_t log2_min_width> struct subdivision_predicate_t
{
    // total noise budget in ulps relative to interval scale
    //
    // The margin is determined roughly by the number of ops per sample and error introduced by rounding after each op.
    // The ops include hermite-to-polynomial basis conversion, cubic Horner, and norm, each contrbuting up to
    // ulps_per_op of error.
    static constexpr auto ops_per_sample = int_t{16};
    static constexpr auto ulps_per_op = int_t{4};
    static constexpr auto relative_noise_margin
        = std::numeric_limits<scalar_t>::epsilon() * scalar_t{ops_per_sample * ulps_per_op};

    scalar_t global_tolerance;

    constexpr auto operator()(auto const& interval) const noexcept -> bool
    {
        auto const noise_floor = interval.residual.scale * relative_noise_margin;
        auto const local_tolerance = std::max(global_tolerance, noise_floor);

        return interval.subdomain.log2_width > log2_min_width && interval.residual.metric_error > local_tolerance;
    }
};

} // namespace crv::spline
