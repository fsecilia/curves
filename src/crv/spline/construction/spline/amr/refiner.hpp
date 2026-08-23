// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/spline/construction/spline/amr/generation_result.hpp>
#include <cassert>

namespace crv::spline {

enum class refinement_requirement_t
{
    none,
    optional,
    required,
};

/// best-first, best-effort adaptive mesh refiner
///
/// Unsafe intervals are mandatory work and remain ahead of quality-only work in the refinement pool. Optional
/// refinement may stop at the segment budget; mandatory refinement must either split or return a construction error.
template <typename typestate_t, typename subdivider_t, typename subdivision_predicate_t, int_t max_segment_count>
struct refiner_t
{
    using interval_t = subdivider_t::interval_t;
    using x_t = interval_t::subdomain_t::x_t;
    using result_t = spline_generation_result_t<x_t>;

    subdivision_predicate_t requires_subdivision;
    subdivider_t subdivide;

    constexpr auto operator()(typestate_t&& state, auto const& target) const -> result_t
    {
        auto& workspace = state.workspace;
        auto& refinement_pool = workspace.refinement_pool;
        auto& completed_intervals = workspace.completed_intervals;
        assert(!refinement_pool.empty() && "refinement_pool must not be empty");
        assert(refinement_pool.size() <= max_segment_count && "refinement_pool overfull");
        assert(completed_intervals.empty() && "completed_intervals must be empty");

        while (!refinement_pool.empty())
        {
            auto const& interval = refinement_pool.top();
            switch (refinement_requirement(interval))
            {
                case refinement_requirement_t::none: complete_top(refinement_pool, completed_intervals); break;

                case refinement_requirement_t::optional:
                    if (segment_budget_full(refinement_pool, completed_intervals))
                    {
                        return drain_remaining_safe(refinement_pool, completed_intervals);
                    }
                    split_top(refinement_pool, target);
                    break;

                case refinement_requirement_t::required:
                    if (!can_bisect(interval))
                    {
                        return failure(spline_generation_error_reason_t::minimum_interval_width, interval);
                    }
                    if (segment_budget_full(refinement_pool, completed_intervals))
                    {
                        return failure(spline_generation_error_reason_t::segment_budget_exhausted, interval);
                    }
                    split_top(refinement_pool, target);
                    break;
            }
        }

        return {};
    }

private:
    constexpr auto refinement_requirement(interval_t const& interval) const noexcept -> refinement_requirement_t
    {
        if (!interval.residual) return refinement_requirement_t::required;
        if (requires_subdivision(interval)) return refinement_requirement_t::optional;
        return refinement_requirement_t::none;
    }

    static constexpr auto can_bisect(interval_t const& interval) noexcept -> bool
    {
        auto const& subdomain = interval.subdomain;
        return subdomain.left_x < subdomain.midpoint_x && subdomain.midpoint_x < subdomain.right_x;
    }

    static constexpr auto segment_budget_full(auto const& refinement_pool, auto const& completed_intervals) noexcept
        -> bool
    {
        return refinement_pool.size() + completed_intervals.size() >= static_cast<std::size_t>(max_segment_count);
    }

    constexpr auto split_top(auto& refinement_pool, auto const& target) const -> void
    {
        // construct both children before mutating the pool
        auto const children = subdivide(target, refinement_pool.top());
        refinement_pool.pop();
        refinement_pool.push(children.left);
        refinement_pool.push(children.right);
    }

    static constexpr auto complete_top(auto& refinement_pool, auto& completed_intervals) -> void
    {
        assert(refinement_pool.top().residual.has_value());
        completed_intervals.push_back(refinement_pool.top());
        refinement_pool.pop();
    }

    static constexpr auto drain_remaining_safe(auto& refinement_pool, auto& completed_intervals) -> result_t
    {
        while (!refinement_pool.empty())
        {
            if (!refinement_pool.top().residual)
            {
                return failure(spline_generation_error_reason_t::segment_budget_exhausted, refinement_pool.top());
            }
            complete_top(refinement_pool, completed_intervals);
        }
        return {};
    }

    static constexpr auto failure(spline_generation_error_reason_t reason, interval_t const& interval) noexcept
        -> result_t
    {
        return {.error = typename result_t::error_t{
                    .reason = reason,
                    .left = interval.subdomain.left_x,
                    .right = interval.subdomain.right_x,
                }};
    }
};

} // namespace crv::spline
