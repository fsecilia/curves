// SPDX-License-Identifier: MIT

/// \file
/// \brief subdomain bisection
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <numeric>

namespace crv::spline {

/// result of bisecting a subdomain
template <typename t_subdomain_t> struct bisection_t
{
    using subdomain_t = t_subdomain_t;

    subdomain_t left;
    subdomain_t right;
};

/// bisects subdomains
template <typename t_bisection_t> struct bisector_t
{
    using bisection_t = t_bisection_t;
    using subdomain_t = bisection_t::subdomain_t;

    constexpr auto operator()(auto const& sample_target_function, subdomain_t const& parent) const noexcept
        -> bisection_t
    {
        using std::midpoint;

        using scalar_t = subdomain_t::scalar_t;
        using jet_t = subdomain_t::jet_t;

        auto const child_log2_width = parent.log2_width - 1;
        return {
            .left = subdomain_t{
                .left = parent.left,
                .midpoint = sample_target_function(jet_t{midpoint(parent.left.x, parent.midpoint.x), scalar_t{1}}),
                .right = parent.midpoint,
                .log2_width = child_log2_width,
            },
            .right = subdomain_t{
                .left = parent.midpoint,
                .midpoint = sample_target_function(jet_t{midpoint(parent.midpoint.x, parent.right.x), scalar_t{1}}),
                .right = parent.right,
                .log2_width = child_log2_width,
            },
        };
    }
};

} // namespace crv::spline
