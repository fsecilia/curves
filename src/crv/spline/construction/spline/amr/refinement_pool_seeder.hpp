// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/spline/construction/segment/amr/transfer_sample.hpp>
#include <expected>
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
    using error_t = interval_factory_t::error_t;
    using next_t = typestate_t::next_t;
    using result_t = std::expected<next_t, error_t>;

    using critical_points_t = std::vector<x_t>;

    [[no_unique_address]] subdomain_factory_t create_subdomain;
    [[no_unique_address]] interval_factory_t create_interval;

    static constexpr auto domain_end = x_t{1} << log2_domain_end;

    constexpr auto operator()(typestate_t&& state, auto const& target, critical_points_t const& critical_points) const
        -> result_t
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
        auto left_sample = sample_transfer(target, jet_t{from_fixed<scalar_t>(left_x), scalar_t{1}});

        for (auto const right_x : critical_points)
        {
            auto seeded = seed_interval(target, left_sample, left_x, right_x, refinement_pool);
            if (!seeded) return std::unexpected{seeded.error()};
            left_sample = *seeded;
            left_x = right_x;
        }

        auto seeded = seed_interval(target, left_sample, left_x, domain_end, refinement_pool);
        if (!seeded) return std::unexpected{seeded.error()};

        return next_t{workspace};
    }

private:
    constexpr auto seed_interval(auto const& target, function_sample_t const& left_sample, x_t left_x, x_t right_x,
        auto& refinement_pool) const -> std::expected<function_sample_t, error_t>
    {
        auto const subdomain = create_subdomain(target, left_sample, left_x, right_x);
        auto interval = create_interval(target, subdomain);
        if (!interval) return std::unexpected{interval.error()};
        refinement_pool.emplace(*interval);
        return subdomain.right;
    }
};

} // namespace crv::spline
