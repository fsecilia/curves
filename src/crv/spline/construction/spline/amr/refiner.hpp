// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <cassert>

namespace crv::spline {

/// best-first adaptive mesh refiner
///
/// The worst pending interval is either split and returned to the pool or moved to completed. Refinement stops when the
/// pool is empty or the segment budget is full.
template <typename typestate_t, typename subdivider_t, typename subdivision_predicate_t, int_t max_segment_count>
struct refiner_t
{
    using interval_t = subdivider_t::interval_t;

    subdivision_predicate_t requires_subdivision;
    subdivider_t subdivide;

    constexpr auto operator()(typestate_t&& state, auto const& target) const -> typename typestate_t::next_t
    {
        auto& workspace = state.workspace;
        auto& refinement_pool = workspace.refinement_pool;
        auto& completed_intervals = workspace.completed_intervals;
        assert(!refinement_pool.empty() && "refinement_pool must not be empty");
        assert(refinement_pool.size() <= max_segment_count && "refinement_pool overfull");
        assert(completed_intervals.empty() && "completed_intervals must be empty");

        subdivide_all(refinement_pool, completed_intervals, target);
        drain_remaining(refinement_pool, completed_intervals);

        return typename typestate_t::next_t{workspace};
    }

private:
    constexpr auto subdivide_all(auto& refinement_pool, auto& completed_intervals, auto const& target) const -> void
    {
        // refine until pool empty or budget full
        while (!refinement_pool.empty() && refinement_pool.size() + completed_intervals.size() < max_segment_count)
        {
            // interval is a reference; pop only after subdivision decision
            auto const& interval = refinement_pool.top();
            if (requires_subdivision(interval))
            {
                auto const children = subdivide(target, interval);
                refinement_pool.pop();
                refinement_pool.push(children.left);
                refinement_pool.push(children.right);
            }
            else
            {
                completed_intervals.push_back(interval);
                refinement_pool.pop();
            }
        }
    }

    constexpr auto drain_remaining(auto& refinement_pool, auto& completed_intervals) const -> void
    {
        // move remaining intervals to completed
        while (!refinement_pool.empty())
        {
            completed_intervals.push_back(refinement_pool.top());
            refinement_pool.pop();
        }
    }
};

} // namespace crv::spline
