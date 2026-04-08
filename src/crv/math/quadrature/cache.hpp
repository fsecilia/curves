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
    using map_t             = std::flat_map<real_t, real_t>;
    using boundaries_t      = map_t::key_container_type;
    using cumulative_sums_t = map_t::mapped_container_type;

    struct interval_t
    {
        real_t left_bound;
        real_t base_integral;

        constexpr auto operator<=>(interval_t const&) const noexcept -> auto = default;
        constexpr auto operator==(interval_t const&) const noexcept -> bool  = default;
    };

    explicit integral_cache_t(map_t intervals) noexcept : intervals_{std::move(intervals)}
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

    auto operator<=>(integral_cache_t const&) const noexcept -> auto = default;
    auto operator==(integral_cache_t const&) const noexcept -> bool  = default;

private:
    map_t intervals_;
};

template <typename real_t, typename integral_cache_t, typename accumulator_t> class integral_cache_builder_t
{
public:
    using boundaries_t      = integral_cache_t::boundaries_t;
    using cumulative_sums_t = integral_cache_t::cumulative_sums_t;

    integral_cache_builder_t()
    {
        boundaries_.reserve(32);
        boundaries_.push_back(real_t{0});

        cumulative_sums_.reserve(32);
        cumulative_sums_.push_back(real_t{0});
    }

    auto append(real_t right_bound, real_t sum) -> void
    {
        assert(right_bound > boundaries_.back() && "integral_cache_t: boundaries must be monotonically increasing");

        running_sum_ += sum;

        boundaries_.push_back(right_bound);
        cumulative_sums_.push_back(static_cast<real_t>(running_sum_));
    }

    auto finalize() && noexcept -> integral_cache_t
    {
        return integral_cache_t{std::flat_map{std::sorted_unique, std::move(boundaries_), std::move(cumulative_sums_)}};
    }

private:
    boundaries_t      boundaries_{};
    cumulative_sums_t cumulative_sums_{};
    accumulator_t     running_sum_{};
};

} // namespace crv::quadrature
