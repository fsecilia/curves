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
concept is_root_subdivider = requires(subdivider_t const& subdivider, real_t value) {
    { subdivider(value, value, value) } -> std::same_as<segment_t<real_t>>;
};

/// callable that constructs a bisection from a parent segment
template <typename subdivider_t, typename real_t>
concept is_bisector = requires(subdivider_t const& subdivider, segment_t<real_t> segment) {
    { subdivider(segment) } -> std::same_as<bisection_t<real_t>>;
};

/// callable that constructs root segments and bisects parent segments
template <typename subdivider_t, typename real_t>
concept is_subdivider = is_root_subdivider<subdivider_t, real_t> && is_bisector<subdivider_t, real_t>;

/// subdivides segments using integrand and rule
template <typename real_t, typename integrand_t, typename rule_t> class subdivider_t
{
public:
    using segment_t   = segment_t<real_t>;
    using bisection_t = bisection_t<real_t>;

    constexpr subdivider_t(integrand_t const& integrand, rule_t const& rule) noexcept
        : integrand_{integrand}, rule_{rule}
    {}

    /// creates root segments from simple ranges
    constexpr auto operator()(real_t left, real_t right, real_t tolerance) const noexcept -> segment_t
    {
        return segment_t{
            .left      = left,
            .right     = right,
            .integral  = rule_(left, right, integrand_).value,
            .tolerance = tolerance,
            .depth     = 0,
        };
    }

    /// bisects segment into two child segments
    constexpr auto operator()(segment_t const& parent) const noexcept -> bisection_t
    {
        auto const parent_midpoint = std::midpoint(parent.left, parent.right);
        auto const child_tolerance = parent.tolerance * real_t{0.5};
        auto const child_depth     = parent.depth + 1;

        auto const left_result  = rule_(parent.left, parent_midpoint, integrand_);
        auto const right_result = rule_(parent_midpoint, parent.right, integrand_);
        assert(left_result.error >= 0 && right_result.error >= 0);

        auto const quadrature_error  = left_result.error + right_result.error;
        auto const combined_integral = left_result.value + right_result.value;
        auto const subdivision_error = abs(combined_integral - parent.integral);

        // clang-format off
        return bisection_t{
            .left_child = segment_t
            {
                .left      = parent.left,
                .right     = parent_midpoint,
                .integral  = left_result.value,
                .tolerance = child_tolerance,
                .depth     = child_depth,
            },
            .right_child = segment_t
            {
                .left      = parent_midpoint,
                .right     = parent.right,
                .integral  = right_result.value,
                .tolerance = child_tolerance,
                .depth     = child_depth,
            },
            .combined_integral = combined_integral,
            .error_estimate    = std::max(quadrature_error, subdivision_error),
        };
        // clang-format on
    }

private:
    integrand_t const& integrand_{};
    rule_t const&      rule_{};
};

} // namespace crv::quadrature
