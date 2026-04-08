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

template <std::floating_point real_t> class stack_t
{
public:
    using segment_t   = segment_t<real_t>;
    using bisection_t = bisection_t<real_t>;

    stack_t() { segments_.reserve(32); }

    auto clear() noexcept -> void { segments_.clear(); }

    auto push(segment_t segment) -> void { segments_.emplace_back(segment); }
    auto push(bisection_t bisection) -> void
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

} // namespace crv::quadrature
