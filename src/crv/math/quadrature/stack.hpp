// SPDX-License-Identifier: MIT

/// \file
/// \brief segment working stack
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/quadrature/bisector.hpp>
#include <crv/ranges.hpp>
#include <cassert>
#include <concepts>

namespace crv::quadrature {

/// seeds empty stack with domain-level segments
class stack_seeder_t
{
public:
    /// seeds stack with single segment across entire domain
    ///
    /// \pre stack.empty()
    template <std::floating_point real_t>
    auto seed(auto& stack, is_root_bisector<real_t> auto const& subdivider, real_t domain_max, real_t global_tolerance)
        -> void
    {
        assert(stack.empty() && "stack_seeder_t: stack must be empty before seeding");

        stack.push_back(subdivider(real_t{0}, domain_max, global_tolerance));
    }

    /// seeds stack with multiple segments, splitting domain at critical points
    ///
    /// critical_points should not include 0 and domain_max; these are implied
    ///
    /// \pre stack.empty()
    /// \pre critical_points are sorted increasing and unique
    /// \pre critical_points in (0, domain_max)
    template <std::floating_point real_t>
    auto seed(auto& stack, is_root_bisector<real_t> auto const& subdivider, real_t domain_max, real_t global_tolerance,
              compatible_range<real_t> auto const& critical_points) -> void
    {
        assert(stack.empty() && "stack_seeder_t: stack must be empty before seeding");

        // push in reverse order so leftmost segment pops first
        auto right = domain_max;
        for (auto const critical_point : critical_points | std::views::reverse)
        {
            auto const left = static_cast<real_t>(critical_point);
            assert((real_t{0} < left && left < domain_max)
                   && "stack_seeder_t: critical points must be in (0, domain_max)");
            assert(left < right && "stack_seeder_t: critical points must be sorted increasing and unique");

            auto const tolerance = global_tolerance * ((right - left) / domain_max);
            stack.push_back(subdivider(left, right, tolerance));

            right = left;
        }

        stack.push_back(subdivider(real_t{0}, right, global_tolerance * (right / domain_max)));
    }
};

} // namespace crv::quadrature
