// SPDX-License-Identifier: MIT

/// \file
/// \brief adaptive subdivision loop
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/abs.hpp>
#include <crv/math/limits.hpp>
#include <crv/math/quadrature/bisector.hpp>
#include <crv/math/quadrature/segment.hpp>
#include <algorithm>

namespace crv::quadrature {
namespace generic {

/// decides whether a segment should be subdivided
template <typename t_real_t> struct subdivision_predicate_t
{
    using real_t = t_real_t;
    using segment_t = segment_t<real_t>;

    static constexpr auto min_width = epsilon<real_t>() * real_t{1024};
    static constexpr auto relative_noise_margin = epsilon<real_t>() * real_t{64};

    constexpr auto operator()(segment_t const& segment, real_t area, real_t error, int_t depth_limit) const noexcept
        -> bool
    {
        auto const current_width = segment.right - segment.left;
        auto const noise_floor = abs(area) * relative_noise_margin;
        auto const local_tolerance = std::max(segment.tolerance, noise_floor);
        return segment.depth < depth_limit && current_width > min_width && error > local_tolerance;
    }
};

/// adaptively subdivides the contents of a segment stack
template <typename t_subdivision_predicate_t> struct subdivider_t
{
    using subdivision_predicate_t = t_subdivision_predicate_t;
    using real_t = subdivision_predicate_t::real_t;

    [[no_unique_address]] subdivision_predicate_t should_subdivide{};

    constexpr auto run(
        auto& stack, is_nested_bisector<real_t> auto const& bisector, auto& builder, int_t depth_limit) const -> void
    {
        while (!stack.empty())
        {
            auto const segment = stack.back();
            stack.pop_back();

            auto const bisection = bisector(segment);

            if (should_subdivide(segment, bisection.integral, bisection.error_estimate, depth_limit))
            {
                // push right then left so left pops first
                stack.push_back(bisection.right);
                stack.push_back(bisection.left);
            }
            else
            {
                builder.append(bisection.right.right, bisection.integral, bisection.error_estimate);
            }
        }
    }
};

} // namespace generic

template <typename real_t> using subdivider_t = generic::subdivider_t<generic::subdivision_predicate_t<real_t>>;

} // namespace crv::quadrature
