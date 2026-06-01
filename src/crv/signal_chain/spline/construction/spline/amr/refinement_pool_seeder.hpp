// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/jet/jet.hpp>
#include <algorithm>
#include <functional>

namespace crv::spline {

/// seeds refinement pool with initial set of interval spans between critical points in the domain
///
/// This type splits the mapped domain using pure bisection to subdivide at a set of critical points.
template <typename typestate_t, typename span_decomposer_t, int_t log2_domain_end> struct refinement_pool_seeder_t
{
    using x_t = span_decomposer_t::x_t;
    using scalar_t = span_decomposer_t::scalar_t;
    using jet_t = span_decomposer_t::jet_t;
    using function_sample_t = span_decomposer_t::function_sample_t;

    using critical_points_t = std::vector<x_t>;

    [[no_unique_address]] span_decomposer_t decompose_span;

    static constexpr auto domain_end = x_t{1} << log2_domain_end;

    constexpr auto operator()(typestate_t&& state, auto const& sample_target_function,
        critical_points_t const& critical_points) const -> typename typestate_t::next_t
    {
        assert(std::ranges::adjacent_find(critical_points, std::greater_equal{}) == critical_points.end()
            && "critical points must be unique and strictly monotonically increasing");
        assert((critical_points.empty() || (critical_points.front() > x_t{0} && critical_points.back() < domain_end))
            && "all critical points must be in (0, domain_end)");

        auto& workspace = state.workspace;
        auto& refinement_pool = workspace.refinement_pool;
        assert(refinement_pool.empty());

        // start at 0
        auto left_critical_point = x_t{0};
        auto left_function_sample = sample_target_function(jet_t{scalar_t{0.0}, scalar_t{1}});

        // proceed through pairs of critical points
        for (auto const& right_critical_point : critical_points)
        {
            left_function_sample = decompose_span(sample_target_function, left_function_sample, left_critical_point,
                right_critical_point, refinement_pool);
            left_critical_point = right_critical_point;
        }

        // finish with end of domain
        decompose_span(sample_target_function, left_function_sample, left_critical_point, domain_end, refinement_pool);

        return typename typestate_t::next_t{workspace};
    }
};

} // namespace crv::spline
