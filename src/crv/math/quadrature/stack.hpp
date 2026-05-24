// SPDX-License-Identifier: MIT

/// \file
/// \brief segment working stack
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/quadrature/integral.hpp>
#include <crv/math/quadrature/segment.hpp>
#include <crv/ranges.hpp>
#include <cassert>
#include <concepts>

namespace crv::quadrature {

/// seeds empty stack with domain-level segments
template <std::floating_point t_scalar_t> class stack_seeder_t
{
public:
    using scalar_t = t_scalar_t;

    /// seeds stack with one segment per subdomain, splitting at critical points
    ///
    /// critical_points should not include 0 and domain_end; these are implied. An empty range yields a single segment
    /// across the full domain.
    ///
    /// \pre stack.empty()
    /// \pre critical_points are sorted increasing and unique
    /// \pre critical_points in (0, domain_end)
    auto seed(auto& stack, is_integral<scalar_t> auto const& integral, scalar_t domain_end, scalar_t global_tolerance,
        compatible_range<scalar_t> auto const& critical_points) -> void
    {
        assert(stack.empty() && "stack_seeder_t: stack must be empty before seeding");

        // push in reverse order so leftmost segment pops first
        auto right = domain_end;
        for (auto const critical_point : critical_points | std::views::reverse)
        {
            auto const left = static_cast<scalar_t>(critical_point);
            assert((scalar_t{0} < left && left < domain_end)
                && "stack_seeder_t: critical points must be in (0, domain_end)");
            assert(left < right && "stack_seeder_t: critical points must be sorted increasing and unique");

            stack.push_back(segment_t<scalar_t>{
                .left = left,
                .right = right,
                .coarse_integral = integral.integrate(left, right),
                .tolerance = global_tolerance * ((right - left) / domain_end),
                .depth = 0,
            });

            right = left;
        }

        stack.push_back(segment_t<scalar_t>{
            .left = scalar_t{0},
            .right = right,
            .coarse_integral = integral.integrate(scalar_t{0}, right),
            .tolerance = global_tolerance * (right / domain_end),
            .depth = 0,
        });
    }
};

} // namespace crv::quadrature
