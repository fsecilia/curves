// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>

#include <algorithm>
#include <cassert>
#include <concepts>
#include <utility>

namespace crv::spline {

template <std::floating_point scalar_t, typename x_t, typename spline_t, typename typestates_t,
    typename refinement_pool_t, typename refinement_pool_seeder_t, typename refiner_t, typename assembler_t>
class spline_generator_t
{
public:
    using critical_points_t = refinement_pool_seeder_t::critical_points_t;
    using workspace_t = typestates_t::workspace_t;

    constexpr spline_generator_t() : spline_generator_t{{}, {}, {}} {}

    constexpr spline_generator_t(refinement_pool_seeder_t seed_refinement_pool, refiner_t refine, assembler_t assemble)
        : spline_generator_t{std::move(seed_refinement_pool), std::move(refine), std::move(assemble), {}}
    {}

    constexpr spline_generator_t(
        refinement_pool_seeder_t seed_refinement_pool, refiner_t refine, assembler_t assemble, workspace_t workspace)
        : seed_refinement_pool_{std::move(seed_refinement_pool)}, refine_{std::move(refine)},
          assemble_{std::move(assemble)}, workspace_{std::move(workspace)}
    {}

    template <typename target_t>
    constexpr auto operator()(auto& spline, target_t&& target, critical_points_t critical_points) -> void
    {
        assert(workspace_.empty());
        workspace_.clear();

        std::ranges::sort(critical_points);
        auto const duplicates = std::ranges::unique(critical_points);
        critical_points.erase(duplicates.begin(), duplicates.end());

        auto unseeded_state = typename typestates_t::initial_t{workspace_};
        auto const& construction_target = target;
        auto unrefined_state = seed_refinement_pool_(std::move(unseeded_state), construction_target, critical_points);
        auto unassembled_state = refine_(std::move(unrefined_state), construction_target);
        assemble_(std::move(unassembled_state), spline);

        assert(workspace_.empty());
    }

private:
    refinement_pool_seeder_t seed_refinement_pool_;
    refiner_t refine_;
    assembler_t assemble_;
    workspace_t workspace_;
};

template <typename policy_t> struct spline_generator_factory_t
{
    using scalar_t = policy_t::scalar_t;
    using product_t = policy_t::spline_generator_t;

    [[nodiscard]] auto operator()(scalar_t global_tolerance) const -> product_t
    {
        return typename policy_t::spline_generator_t{typename policy_t::refinement_pool_seeder_t{},
            typename policy_t::refiner_t{
                .requires_subdivision =
                    typename policy_t::subdivision_predicate_t{.global_tolerance = global_tolerance},
                .subdivide = {},
            },
            typename policy_t::assembler_t{
                .sort_intervals = {},
                .unzip_intervals = {},
                .pad_keys = {},
                .extend_tangent =
                    typename policy_t::tangent_extender_t{.y_limit = policy_t::y_limit, .extract_float = {}},
            }};
    }
};

} // namespace crv::spline
