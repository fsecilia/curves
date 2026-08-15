// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/jet/jet.hpp>
#include <vector>

namespace crv::spline {

/// seeds one initial interval directly between each pair of supplied knots
///
/// Supplied knot positions are exact fixed-point geometry. They are not aligned to min_width.
template <typename typestate_t, typename subdomain_factory_t, typename interval_factory_t, int_t max_segment_count,
    int_t log2_domain_end>
struct refinement_pool_seeder_t
{
    using x_t = subdomain_factory_t::x_t;
    using scalar_t = subdomain_factory_t::scalar_t;
    using jet_t = subdomain_factory_t::jet_t;
    using function_sample_t = subdomain_factory_t::function_sample_t;

    using critical_points_t = std::vector<x_t>;

    [[no_unique_address]] subdomain_factory_t create_subdomain;
    [[no_unique_address]] interval_factory_t create_interval;

    static constexpr auto domain_end = x_t{1} << log2_domain_end;

    constexpr auto operator()(typestate_t&& state, auto const& sample_target_function,
        critical_points_t const& critical_points) const -> typename typestate_t::next_t
    {
        assert(std::ranges::adjacent_find(critical_points, std::greater_equal{}) == critical_points.end()
            && "critical points must be unique and strictly monotonically increasing");
        assert((critical_points.empty() || (critical_points.front() > x_t{0} && critical_points.back() < domain_end))
            && "all critical points must be in (0, domain_end)");
        assert(int_cast<int_t>(critical_points.size()) + 1 <= max_segment_count
            && "critical point partitioning exceeded segment budget");

        auto& workspace = state.workspace;
        auto& refinement_pool = workspace.refinement_pool;
        assert(refinement_pool.empty());

        auto left_x = x_t{0};
        auto left_sample = sample_target_function(jet_t{from_fixed<scalar_t>(left_x), scalar_t{1}});

        for (auto const right_x : critical_points)
        {
            left_sample = seed_interval(sample_target_function, left_sample, left_x, right_x, refinement_pool);
            left_x = right_x;
        }

        seed_interval(sample_target_function, left_sample, left_x, domain_end, refinement_pool);

        return typename typestate_t::next_t{workspace};
    }

private:
    constexpr auto seed_interval(auto const& sample_target_function, function_sample_t const& left_sample, x_t left_x,
        x_t right_x, auto& refinement_pool) const -> function_sample_t
    {
        auto const subdomain = create_subdomain(sample_target_function, left_sample, left_x, right_x);
        refinement_pool.emplace(create_interval(sample_target_function, subdomain));
        return subdomain.right;
    }
};

} // namespace crv::spline
