// SPDX-License-Identifier: MIT

/// \file
/// \brief cached result of quadrature
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <cassert>
#include <flat_map>
#include <iterator>

namespace crv::quadrature {

/// discrete partition and cumulative integral
template <typename real_t> class integral_cache_t
{
public:
    struct interval_t
    {
        real_t left_bound;
        real_t base_integral;

        constexpr auto operator<=>(interval_t const&) const noexcept -> auto = default;
        constexpr auto operator==(interval_t const&) const noexcept -> bool  = default;
    };

    explicit integral_cache_t(std::flat_map<real_t, real_t> intervals) : intervals_{std::move(intervals)}
    {
        assert(!intervals_.empty() && "integral_cache_t: empty cache provided");
        assert(intervals_.begin()->first == real_t{0} && "integral_cache_t: origin must start at 0");
        assert(intervals_.begin()->second == real_t{0} && "integral_cache_t: cumulative sum must start at 0");
    }

    /// locates the interval containing given location
    auto interval(real_t location) const -> interval_t
    {
        assert(intervals_.keys().front() <= location && location <= intervals_.keys().back()
               && "integral_cache_t: domain error");

        auto const right = intervals_.upper_bound(location);
        auto const left  = std::ranges::prev(right);
        return {left->first, left->second};
    }

private:
    std::flat_map<real_t, real_t> intervals_;
};

} // namespace crv::quadrature
