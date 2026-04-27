// SPDX-License-Identifier: MIT

/// \file
/// \brief segment building and refinement
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/abs.hpp>
#include <crv/math/quadrature/segment.hpp>
#include <cassert>
#include <numeric>

namespace crv::quadrature {

/// callable that constructs a root segment from (left, right, tolerance)
template <typename refiner_t, typename real_t>
concept is_root_refiner = requires(refiner_t const& refiner, real_t value) {
    { refiner.evaluate(value, value, value) } -> std::same_as<segment_t<real_t>>;
};

/// callable that refines a parent segment into two children and the parent's refined estimate
template <typename refiner_t, typename real_t>
concept is_refiner = requires(refiner_t const& refiner, segment_t<real_t> segment) {
    { refiner.refine(segment) } -> std::same_as<refinement_t<real_t>>;
};

/// applies a rule to build initial segments and to refine parent segments into level-N+1 estimates
template <typename t_integral_t> class refiner_t
{
public:
    using integral_t = t_integral_t;
    using real_t = integral_t::real_t;
    using segment_t = segment_t<real_t>;
    using refinement_t = refinement_t<real_t>;

    constexpr refiner_t(integral_t integral) noexcept : integral_{std::move(integral)} {}

    /// builds an initial segment by integrating over [left, right]
    constexpr auto evaluate(real_t left, real_t right, real_t tolerance) const noexcept -> segment_t
    {
        return segment_t{left, right, integral_.integrate(left, right), tolerance, 0};
    }

    /// refines segment into two child segments and the parent's level-N+1 integral and error
    constexpr auto refine(segment_t const& parent) const noexcept -> refinement_t
    {
        auto const parent_midpoint = std::midpoint(parent.left, parent.right);
        auto const child_tolerance = parent.tolerance / 2;
        auto const child_depth = parent.depth + 1;

        auto const left_rule_result = integral_.estimate(parent.left, parent_midpoint);
        auto const right_rule_result = integral_.estimate(parent_midpoint, parent.right);

        auto const refined_integral = left_rule_result.sum + right_rule_result.sum;
        auto const quadrature_error = left_rule_result.error + right_rule_result.error;
        auto const subdivision_error = abs(refined_integral - parent.coarse_integral);
        auto const refined_error = std::max(quadrature_error, subdivision_error);

        // clang-format off
        return refinement_t
        {
            .left = segment_t
            {
                .left = parent.left,
                .right = parent_midpoint,
                .coarse_integral = left_rule_result.sum,
                .tolerance = child_tolerance,
                .depth = child_depth,
            },
            .right = segment_t
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

    constexpr auto finalize() && noexcept -> integral_t { return std::move(integral_); }

private:
    integral_t integral_{};
};

} // namespace crv::quadrature
