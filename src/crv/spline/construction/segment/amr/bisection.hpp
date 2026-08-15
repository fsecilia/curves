// SPDX-License-Identifier: MIT

/// \file
/// \brief subdomain bisection
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/spline/construction/segment/amr/transfer_sample.hpp>
#include <numeric>

namespace crv::spline {

/// result of bisecting a subdomain
template <typename t_subdomain_t> struct bisection_t
{
    using subdomain_t = t_subdomain_t;

    subdomain_t left;
    subdomain_t right;
};

/// bisects subdomains at an exact representable fixed-point midpoint
template <typename t_bisection_t> struct bisector_t
{
    using bisection_t = t_bisection_t;
    using subdomain_t = bisection_t::subdomain_t;
    using x_t = subdomain_t::x_t;

    constexpr auto operator()(auto const& target, subdomain_t const& parent) const noexcept -> bisection_t
    {
        using std::midpoint;

        using scalar_t = subdomain_t::scalar_t;
        using jet_t = subdomain_t::jet_t;

        auto const split = parent.midpoint_x;
        assert(parent.left_x < split && split < parent.right_x && "subdomain has no distinct representable midpoint");

        auto const left_midpoint_x = x_t::literal(std::midpoint(parent.left_x.value, split.value));
        auto const right_midpoint_x = x_t::literal(std::midpoint(split.value, parent.right_x.value));

        return {
            .left = subdomain_t{
                .left_x = parent.left_x,
                .midpoint_x = left_midpoint_x,
                .right_x = split,
                .left = parent.left,
                .midpoint = sample_transfer(target, jet_t{from_fixed<scalar_t>(left_midpoint_x), scalar_t{1}}),
                .right = parent.midpoint,
            },
            .right = subdomain_t{
                .left_x = split,
                .midpoint_x = right_midpoint_x,
                .right_x = parent.right_x,
                .left = parent.midpoint,
                .midpoint = sample_transfer(target, jet_t{from_fixed<scalar_t>(right_midpoint_x), scalar_t{1}}),
                .right = parent.right,
            },
        };
    }
};

} // namespace crv::spline
