// SPDX-License-Identifier: MIT

/// \file
/// \brief mutable workspace for stateless spliner
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/spline/construction/interval.hpp>
#include <crv/priority_queue.hpp>
#include <vector>

namespace crv::spline {

/// mutable state for adaptive mesh refinement
template <typename interval_t, int_t max_segments> struct workspace_t
{
    std::vector<interval_t> completed_intervals;
    priority_queue_t<std::vector<interval_t>, interval_priority_less_t> refinement_pool;

    constexpr auto clear() noexcept -> void
    {
        completed_intervals.clear();
        refinement_pool.clear();
    }

    constexpr auto empty() const noexcept -> bool { return completed_intervals.empty() && refinement_pool.empty(); }

    constexpr workspace_t()
    {
        completed_intervals.reserve(max_segments);
        refinement_pool.reserve(max_segments);
    }
};

} // namespace crv::spline
