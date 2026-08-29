// SPDX-License-Identifier: MIT

/// \file
/// \brief cached quadrature prefixes for antiderivative evaluation
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <algorithm>
#include <cassert>
#include <concepts>
#include <utility>
#include <vector>

namespace crv::quadrature {

/// cached quadrature boundaries and cumulative sums
template <std::floating_point t_scalar_t> class antiderivative_cache_t
{
public:
    using scalar_t = t_scalar_t;

    struct lookup_result_t
    {
        scalar_t boundary;
        scalar_t cumulative_sum;

        constexpr auto operator==(lookup_result_t const&) const noexcept -> bool = default;
    };

    constexpr antiderivative_cache_t(std::vector<scalar_t> boundaries, std::vector<scalar_t> cumulative_sums) noexcept
        : boundaries_{std::move(boundaries)}, cumulative_sums_{std::move(cumulative_sums)}
    {
        assert(!boundaries_.empty() && "antiderivative_cache_t: empty intervals");
        assert(boundaries_.size() == cumulative_sums_.size()
            && "antiderivative_cache_t: interval arrays must have equal sizes");
        assert(boundaries_.front() == scalar_t{0} && "antiderivative_cache_t: origin must start at 0");
        assert(cumulative_sums_.front() == scalar_t{0} && "antiderivative_cache_t: cumulative sum must start at 0");

        for (auto i = std::size_t{1}; i < boundaries_.size(); ++i)
            assert(boundaries_[i - 1] < boundaries_[i]
                && "antiderivative_cache_t: boundaries must be strictly increasing");
    }

    constexpr auto operator==(antiderivative_cache_t const&) const noexcept -> bool = default;

    /// finds the cached prefix whose boundary is at or immediately before x
    constexpr auto lookup(scalar_t x) const noexcept -> lookup_result_t
    {
        assert(contains(x) && "antiderivative_cache_t: domain error");

        // x >= front(), so upper_bound() is always past begin()
        auto const right = std::ranges::upper_bound(boundaries_, x);
        auto const index = static_cast<std::size_t>(right - boundaries_.begin() - 1);
        return {.boundary = boundaries_[index], .cumulative_sum = cumulative_sums_[index]};
    }

    constexpr auto contains(scalar_t x) const noexcept -> bool
    {
        return boundaries_.front() <= x && x <= boundaries_.back();
    }

    /// right edge of the cached domain
    constexpr auto domain_end() const noexcept -> scalar_t { return boundaries_.back(); }

    /// number of accepted quadrature segments; cache also stores the origin
    constexpr auto segment_count() const noexcept -> int_t { return static_cast<int_t>(boundaries_.size() - 1); }

private:
    std::vector<scalar_t> boundaries_;
    std::vector<scalar_t> cumulative_sums_;
};

} // namespace crv::quadrature
