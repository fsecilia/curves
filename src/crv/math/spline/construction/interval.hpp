// SPDX-License-Identifier: MIT

/// \file
/// \brief adaptive spline interval
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/spline/construction/defect_analyzer.hpp>
#include <crv/math/spline/construction/function_sampler.hpp>
#include <crv/math/spline/construction/residual_estimator.hpp>
#include <compare>

namespace crv::spline {

/// unit of work over subdomain
template <std::floating_point t_real_t, typename t_segment_t> struct interval_t
{
    using real_t = t_real_t;
    using segment_t = t_segment_t;

    using residual_t = residual_t<real_t>;

    using function_sample_t = function_sample_t<real_t>;
    function_sample_t left;
    function_sample_t midpoint;
    function_sample_t right;

    segment_defects_t segment_defects;
    residual_t residual;
    segment_t segment;

    /// orders by segment_defects, residual.weighted_error, and left.x
    constexpr auto operator<=>(interval_t const& src) const noexcept -> std::partial_ordering
    {
        auto const lhs_must_subdivide = segment_defects != segment_defects_t{0};
        auto const rhs_must_subdivide = src.segment_defects != segment_defects_t{0};
        if (auto const cmp = lhs_must_subdivide <=> rhs_must_subdivide; std::is_neq(cmp)) return cmp;
        if (auto const cmp = residual.weighted_error <=> src.residual.weighted_error; std::is_neq(cmp)) return cmp;
        return left.x <=> src.left.x;
    }
    constexpr auto operator==(interval_t const& src) const noexcept -> bool = default;
};

} // namespace crv::spline
