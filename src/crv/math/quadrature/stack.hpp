// SPDX-License-Identifier: MIT

/// \file
/// \brief segment working stack
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/quadrature/segment.hpp>
#include <crv/ranges.hpp>
#include <cassert>
#include <concepts>
#include <vector>

namespace crv::quadrature {

/// stack that accepts segments and bisections
template <typename stack_t, typename real_t>
concept is_stack = requires(stack_t& stack, segment_t<real_t> segment, bisection_t<real_t> bisection) {
    stack.push(segment);
    stack.push(bisection);
    { stack.empty() } -> std::convertible_to<bool>;
};

/// working stack of segments in strictly left-to-right order
template <std::floating_point real_t> class stack_t
{
public:
    using segment_t   = segment_t<real_t>;
    using bisection_t = bisection_t<real_t>;

    stack_t() { segments_.reserve(32); }

    auto clear() noexcept -> void { segments_.clear(); }

    auto push(segment_t segment) -> void { segments_.push_back(segment); }
    auto push(bisection_t const& bisection) -> void
    {
        // push right then left so left pops first
        push(bisection.right_child);
        push(bisection.left_child);
    }

    auto pop() noexcept -> segment_t
    {
        assert(!segments_.empty() && "stack_t: pop on empty");
        auto result = segments_.back();
        segments_.pop_back();
        return result;
    }

    auto empty() const noexcept -> bool { return segments_.empty(); }

private:
    std::vector<segment_t> segments_{};
};

/// seeds empty stack with domain-level segments
template <std::floating_point real_t> class stack_seeder_t
{
public:
    using segment_t   = segment_t<real_t>;
    using bisection_t = bisection_t<real_t>;

    /// seeds stack with single segment across entire domain
    ///
    /// \pre stack.empty()
    auto seed(auto& stack, auto& subdivider, real_t domain_max, real_t global_tolerance) -> void
    {
        assert(stack.empty() && "stack_seeder_t: stack must be empty before seeding");

        stack.push(subdivider(real_t{0}, domain_max, global_tolerance));
    }

    /// seeds stack with multiple segments, splitting domain at critical points
    ///
    /// critical_points should not include 0 and domain_max; these are implied
    ///
    /// \pre stack.empty()
    /// \pre critical_points are sorted increasing and unique
    /// \pre critical_points in (0, domain_max)
    auto seed(auto& stack, auto& subdivider, real_t domain_max, real_t global_tolerance,
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
            stack.push(subdivider(left, right, tolerance));

            right = left;
        }

        stack.push(subdivider(real_t{0}, right, global_tolerance * (right / domain_max)));
    }
};

} // namespace crv::quadrature
