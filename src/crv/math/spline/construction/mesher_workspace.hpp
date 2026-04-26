// SPDX-License-Identifier: MIT

/// \file
/// \brief mutable workspace for stateless mesher
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/spline/construction/priority_queue.hpp>
#include <vector>

namespace crv {
namespace generic {

/// mutable state for adaptive mesh refinement
///
/// This type encapsulates mutable state and separates memory allocation from the meshing logic. This way, the
/// allocations can be done once and the buffers maintained between mesh generations while keeping the logic stateless.
template <typename result_t, typename queue_t> struct mesher_workspace_t
{
    result_t result;
    queue_t queue;

    constexpr auto clear() noexcept -> void
    {
        result.clear();
        queue.clear();
    }

    constexpr auto reserve(std::size_t max_segments) -> void
    {
        result.reserve(max_segments);
        queue.reserve(max_segments);
    }
};

} // namespace generic

template <typename segment_t, typename queue_pred_t>
using mesher_workspace_t
    = generic::mesher_workspace_t<std::vector<segment_t>, priority_queue_t<std::vector<segment_t>, queue_pred_t>>;

} // namespace crv
