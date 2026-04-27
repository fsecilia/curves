// SPDX-License-Identifier: MIT

/// \file
/// \brief segment building and refinement
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/abs.hpp>
#include <crv/math/quadrature/integral.hpp>
#include <crv/math/quadrature/segment.hpp>
#include <algorithm>
#include <cassert>
#include <numeric>

namespace crv::quadrature {

/// callable that refines a parent segment into two children and the parent's refined estimate
template <typename bisector_t, typename integral_t, typename real_t>
concept is_bisector = requires(bisector_t const& bisector, integral_t const& integral, segment_t<real_t> segment) {
    { bisector(integral, segment) } -> std::same_as<refinement_t<real_t>>;
};

/// bisects a parent segment into two children and the parent's level-N+1 estimate
struct bisector_t
{
    template <std::floating_point real_t>
    constexpr auto operator()(is_integral<real_t> auto const& integral, segment_t<real_t> const& parent) const noexcept
        -> refinement_t<real_t>
    {
        auto const parent_midpoint = std::midpoint(parent.left, parent.right);
        auto const child_tolerance = parent.tolerance / 2;
        auto const child_depth = parent.depth + 1;

        auto const left_rule_result = integral.estimate(parent.left, parent_midpoint);
        auto const right_rule_result = integral.estimate(parent_midpoint, parent.right);

        auto const refined_integral = left_rule_result.sum + right_rule_result.sum;
        auto const quadrature_error = left_rule_result.error + right_rule_result.error;
        auto const subdivision_error = abs(refined_integral - parent.coarse_integral);
        auto const refined_error = std::max(quadrature_error, subdivision_error);

        // clang-format off
        return refinement_t<real_t>
        {
            .left = segment_t<real_t>
            {
                .left = parent.left,
                .right = parent_midpoint,
                .coarse_integral = left_rule_result.sum,
                .tolerance = child_tolerance,
                .depth = child_depth,
            },
            .right = segment_t<real_t>
            {
                .left = parent_midpoint,
                .right = parent.right,
                .coarse_integral = right_rule_result.sum,
                .tolerance = child_tolerance,
                .depth = child_depth,
            },
            .refined_integral = refined_integral,
            .refined_error = refined_error,
        };
        // clang-format on
    }
};

} // namespace crv::quadrature
