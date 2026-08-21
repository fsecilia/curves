// SPDX-License-Identifier: MIT

/// \file
/// \brief adaptive subdivision loop
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/abs.hpp>
#include <crv/math/limits.hpp>
#include <crv/quadrature/bisector.hpp>
#include <crv/quadrature/integral.hpp>
#include <crv/quadrature/segment.hpp>

namespace crv::quadrature {
namespace generic {

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
    using segment_t = segment_t<scalar_t>;

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
template <typename t_refinement_predicate_t> struct subdivider_t
{
    using refinement_predicate_t = t_refinement_predicate_t;
    using scalar_t = refinement_predicate_t::scalar_t;

    [[no_unique_address]] refinement_predicate_t should_refine{};

    template <is_integral<scalar_t> integral_t>
    constexpr auto run(auto& stack, integral_t const& integral, is_bisector<integral_t, scalar_t> auto const& bisect,
        auto& builder, int_t depth_limit) const -> void
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
                auto const refinement_limited = [&] constexpr noexcept {
                    if constexpr (requires { decision.refinement_limited; }) return decision.refinement_limited;
                    else return false;
                }();
                builder.append(
                    refinement.right.right, refinement.refined_integral, refinement.refined_error, refinement_limited);
            }
        }
    }
};

} // namespace generic

template <typename scalar_t> using subdivider_t = generic::subdivider_t<generic::refinement_predicate_t<scalar_t>>;

} // namespace crv::quadrature
