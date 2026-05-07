// SPDX-License-Identifier: MIT

/// \file
/// \brief adaptive spline interval
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/spline/construction/defect_analyzer.hpp>
#include <crv/math/spline/construction/function_sampler.hpp>
#include <crv/math/spline/construction/residual_estimator.hpp>
#include <tuple>

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
};

/// orders by segment_defects, residual.weighted_error, then left.x
struct interval_priority_less_t
{
    template <typename interval_t>
    constexpr auto operator()(interval_t const& lhs, interval_t const& rhs) const noexcept -> bool
    {
        using std::isfinite;
        assert(isfinite(lhs.residual.weighted_error));
        assert(isfinite(lhs.left.x));
        assert(isfinite(rhs.residual.weighted_error));
        assert(isfinite(rhs.left.x));

        auto const lhs_defects = lhs.segment_defects != segment_defects_t{0};
        auto const rhs_defects = rhs.segment_defects != segment_defects_t{0};

        // tie applies lexicographical compare
        return std::tie(lhs_defects, lhs.residual.weighted_error, lhs.left.x)
            < std::tie(rhs_defects, rhs.residual.weighted_error, rhs.left.x);
    }
};

} // namespace crv::spline
