// SPDX-License-Identifier: MIT

/// \file
/// \brief adaptive subdivision loop
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/abs.hpp>
#include <crv/quadrature/construction/segment.hpp>
#include <crv/quadrature/integral.hpp>
#include <concepts>
#include <limits>

namespace crv::quadrature::construction {

/// outcome of applying the adaptive refinement policy
struct refinement_decision_t
{
    bool refine;
    bool refinement_limited;

    constexpr explicit operator bool() const noexcept { return refine; }
    constexpr auto operator==(refinement_decision_t const&) const noexcept -> bool = default;
};

/// decides whether a segment should be refined further
template <typename t_scalar_t> struct refinement_predicate_t
{
    using scalar_t = t_scalar_t;
    using segment_t = construction::segment_t<scalar_t>;

    static constexpr auto epsilon = std::numeric_limits<scalar_t>::epsilon();
    static constexpr auto min_width = epsilon * scalar_t{1024};
    static constexpr auto relative_noise_margin = epsilon * scalar_t{64};

    constexpr auto operator()(segment_t const& segment, scalar_t area, scalar_t error, int_t depth_limit) const noexcept
        -> refinement_decision_t
    {
        auto const current_width = segment.right - segment.left;
        auto const noise_floor = abs(area) * relative_noise_margin;
        auto const local_tolerance = max(segment.tolerance, noise_floor);
        auto const converged = error <= local_tolerance;
        auto const structurally_limited = !converged && (segment.depth >= depth_limit || current_width <= min_width);

        return {
            .refine = !converged && !structurally_limited,
            .refinement_limited = structurally_limited,
        };
    }
};

/// adaptively subdivides the contents of a segment stack
template <std::floating_point t_scalar_t, typename t_refinement_predicate_t, typename t_bisector_t> struct subdivider_t
{
    using scalar_t = t_scalar_t;
    using refinement_predicate_t = t_refinement_predicate_t;
    using bisector_t = t_bisector_t;

    [[no_unique_address]] refinement_predicate_t should_refine{};
    [[no_unique_address]] bisector_t bisect{};

    template <quadrature::is_integral<scalar_t> integral_t>
    constexpr auto run(auto& stack, integral_t const& integral, auto& builder, int_t depth_limit) const -> void
    {
        while (!stack.empty())
        {
            auto const segment = stack.back();
            stack.pop_back();

            auto const refinement = bisect(integral, segment);
            auto const decision
                = should_refine(segment, refinement.refined_integral, refinement.refined_error, depth_limit);

            if (decision)
            {
                // push right then left so left pops first
                stack.push_back(refinement.right);
                stack.push_back(refinement.left);
            }
            else
            {
                builder.append(refinement.right.right, refinement.refined_integral, refinement.refined_error,
                    decision.refinement_limited);
            }
        }
    }
};

} // namespace crv::quadrature::construction
