// SPDX-License-Identifier: MIT

/// \file
/// \brief constructs cached antiderivative prefixes from accepted quadrature segments
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/quadrature/antiderivative_cache.hpp>
#include <cassert>
#include <concepts>
#include <utility>
#include <vector>

namespace crv::quadrature::construction {

/// accumulates accepted quadrature segments into an antiderivative cache
template <std::floating_point t_scalar_t, typename t_accumulator_t> class antiderivative_cache_builder_t
{
public:
    using scalar_t = t_scalar_t;
    using accumulator_t = t_accumulator_t;
    using cache_t = quadrature::antiderivative_cache_t<scalar_t>;

    struct result_t
    {
        cache_t cache;
        scalar_t achieved_error;
        scalar_t max_error;
        bool refinement_limited = false;

        constexpr auto operator==(result_t const&) const noexcept -> bool = default;
    };

    constexpr antiderivative_cache_builder_t()
    {
        boundaries_.reserve(32);
        boundaries_.push_back(scalar_t{0});

        cumulative_sums_.reserve(32);
        cumulative_sums_.push_back(scalar_t{0});
    }

    constexpr auto append(scalar_t right_bound, scalar_t area, scalar_t error, bool refinement_limited = false) -> void
    {
        assert(right_bound > boundaries_.back()
            && "antiderivative_cache_builder_t: boundaries must be monotonically increasing");

        running_area_ += area;
        running_error_ += error;
        max_error_ = max(max_error_, error);
        refinement_limited_ |= refinement_limited;

        boundaries_.push_back(right_bound);
        cumulative_sums_.push_back(static_cast<scalar_t>(running_area_));
    }

    constexpr auto finalize() && noexcept -> result_t
    {
        return {
            .cache = cache_t{std::move(boundaries_), std::move(cumulative_sums_)},
            .achieved_error = static_cast<scalar_t>(running_error_),
            .max_error = max_error_,
            .refinement_limited = refinement_limited_,
        };
    }

private:
    std::vector<scalar_t> boundaries_;
    std::vector<scalar_t> cumulative_sums_;
    accumulator_t running_area_{};
    accumulator_t running_error_{};
    scalar_t max_error_{};
    bool refinement_limited_{};
};

/// creates fresh one-shot antiderivative cache builders
template <std::floating_point t_scalar_t, typename t_accumulator_t> struct antiderivative_cache_builder_factory_t
{
    using builder_t = antiderivative_cache_builder_t<t_scalar_t, t_accumulator_t>;

    constexpr auto operator()() const -> builder_t { return {}; }
};

} // namespace crv::quadrature::construction
