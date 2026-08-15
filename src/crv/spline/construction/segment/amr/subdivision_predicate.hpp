// SPDX-License-Identifier: MIT

/// \file
/// \brief convergence test
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <algorithm>
#include <concepts>
#include <limits>

namespace crv::spline {

/// decides if an interval should subdivide
///
/// min_width constrains only children created by AMR. Existing interval geometry is never snapped or rejected just
/// because it is narrower than min_width.
template <std::floating_point scalar_t, is_fixed t_x_t, int_t log2_min_width> struct subdivision_predicate_t
{
    using x_t = t_x_t;

    static_assert(x_t::frac_bits + log2_min_width >= 0, "x_t precision cannot represent min_width");

    static constexpr auto min_width = log2_min_width >= 0 ? (x_t{1} << log2_min_width) : (x_t{1} >> -log2_min_width);

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

        return can_subdivide(interval.subdomain) && interval.residual.metric_error > local_tolerance;
    }

private:
    static constexpr auto can_subdivide(auto const& subdomain) noexcept -> bool
    {
        auto const midpoint = subdomain.midpoint_x;
        if (!(subdomain.left_x < midpoint && midpoint < subdomain.right_x)) return false;

        return midpoint - subdomain.left_x >= min_width && subdomain.right_x - midpoint >= min_width;
    }
};

} // namespace crv::spline
