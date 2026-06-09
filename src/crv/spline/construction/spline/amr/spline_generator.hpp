// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/spline/construction/segment/amr/function_sampler.hpp>
#include <concepts>
#include <utility>

namespace crv::spline {

template <std::floating_point scalar_t, typename x_t, typename spline_t, typename typestates_t,
    typename critical_point_conditioner_t, typename refinement_pool_t, typename refinement_pool_seeder_t,
    typename refiner_t, typename assembler_t>
class spline_generator_t
{
public:
    using critical_points_t = critical_point_conditioner_t::critical_points_t;
    using workspace_t = typestates_t::workspace_t;

    constexpr spline_generator_t() : spline_generator_t{{}, {}, {}} {}

    constexpr spline_generator_t(refinement_pool_seeder_t seed_refinement_pool, refiner_t refine, assembler_t assemble)
        : spline_generator_t{{}, std::move(seed_refinement_pool), std::move(refine), std::move(assemble), {}}
    {}

    constexpr spline_generator_t(critical_point_conditioner_t critical_point_conditioner,
        refinement_pool_seeder_t seed_refinement_pool, refiner_t refine, assembler_t assemble, workspace_t workspace)
        : critical_point_conditioner_{std::move(critical_point_conditioner)},
          seed_refinement_pool_{std::move(seed_refinement_pool)}, refine_{std::move(refine)},
          assemble_{std::move(assemble)}, workspace_{std::move(workspace)}
    {}

    template <typename target_function_t>
    constexpr auto operator()(auto& spline, function_sampler_t<target_function_t> sample_target_function,
        critical_points_t critical_points) -> void
    {
        assert(workspace_.empty());
        workspace_.clear();

        critical_points = critical_point_conditioner_(std::move(critical_points));

        auto unseeded_state = typename typestates_t::initial_t{workspace_};
        auto unrefined_state
            = seed_refinement_pool_(std::move(unseeded_state), sample_target_function, std::move(critical_points));
        auto unassembled_state = refine_(std::move(unrefined_state), sample_target_function);
        assemble_(std::move(unassembled_state), spline);

        assert(workspace_.empty());
    }

private:
    critical_point_conditioner_t critical_point_conditioner_;
    refinement_pool_seeder_t seed_refinement_pool_;
    refiner_t refine_;
    assembler_t assemble_;
    workspace_t workspace_;
};

} // namespace crv::spline
