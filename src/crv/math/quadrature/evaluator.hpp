// SPDX-License-Identifier: MIT

/// \file
/// \brief segment subdivision
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/abs.hpp>
#include <crv/math/quadrature/segment.hpp>
#include <cassert>
#include <numeric>

namespace crv::quadrature {

/// callable that constructs a root segment from (left, right, tolerance)
template <typename subdivider_t, typename real_t>
concept is_root_evaluator = requires(subdivider_t const& subdivider, real_t value) {
    { subdivider(value, value, value) } -> std::same_as<segment_t<real_t>>;
};

/// callable that constructs a bisection from a parent segment
template <typename subdivider_t, typename real_t>
concept is_nested_evaluator = requires(subdivider_t const& subdivider, segment_t<real_t> segment) {
    { subdivider(segment) } -> std::same_as<bisection_t<real_t>>;
};

/// callable that constructs root segments and bisects parent segments
template <typename subdivider_t, typename real_t>
concept is_evaluator = is_root_evaluator<subdivider_t, real_t> && is_nested_evaluator<subdivider_t, real_t>;

/// subdivides segments using integrand and rule
template <typename t_integral_t> class evaluator_t
{
public:
    using integral_t = t_integral_t;
    using real_t = integral_t::real_t;
    using segment_t = segment_t<real_t>;
    using bisection_t = bisection_t<real_t>;

    constexpr evaluator_t(integral_t integral) noexcept : integral_{std::move(integral)} {}

    /// creates a root segment from simple range, integrating over [left, right]
    constexpr auto operator()(real_t left, real_t right, real_t tolerance) const noexcept -> segment_t
    {
        return segment_t{left, right, integral_.integrate(left, right), tolerance, 0};
    }

    /// bisects segment into two child segments
    constexpr auto operator()(segment_t const& parent) const noexcept -> bisection_t
    {
        auto const parent_midpoint = std::midpoint(parent.left, parent.right);
        auto const child_tolerance = parent.tolerance / 2;
        auto const child_depth = parent.depth + 1;

        auto const left_rule_result = integral_.estimate(parent.left, parent_midpoint);
        auto const right_rule_result = integral_.estimate(parent_midpoint, parent.right);

        auto const combined_integral = left_rule_result.sum + right_rule_result.sum;
        auto const quadrature_error = left_rule_result.error + right_rule_result.error;
        auto const subdivision_error = abs(combined_integral - parent.integral);
        auto const error_estimate = std::max(quadrature_error, subdivision_error);

        // clang-format off
        return bisection_t
        {
            .left = segment_t
            {
                .left = parent.left,
                .right = parent_midpoint,
                .integral = left_rule_result.sum,
                .tolerance = child_tolerance,
                .depth = child_depth,
            },
            .right = segment_t
            {
                .left = parent_midpoint,
                .right = parent.right,
                .integral = right_rule_result.sum,
                .tolerance = child_tolerance,
                .depth = child_depth,
            },
            .integral = combined_integral,
            .error_estimate = error_estimate,
        };
        // clang-format on
    }

    constexpr auto finalize() && noexcept -> integral_t { return std::move(integral_); }

private:
    integral_t integral_{};
};

} // namespace crv::quadrature
