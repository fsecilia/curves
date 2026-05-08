// SPDX-License-Identifier: MIT

/// \file
/// \brief adaptive spline unit of work
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/spline/construction/defect_analyzer.hpp>
#include <crv/math/spline/construction/function_sampler.hpp>
#include <crv/math/spline/construction/residual_estimator.hpp>
#include <tuple>

namespace crv::spline {

/// geometry of a refinement subdomain
///
/// This type brackets the subdomain [left, right]. It includes log2_width and samples for left, right, and midpoint.
template <std::floating_point real_t> struct subdomain_t
{
    using function_sample_t = function_sample_t<real_t>;

    function_sample_t left;
    function_sample_t midpoint;
    function_sample_t right;
    int_t log2_width;

    constexpr auto operator==(subdomain_t const&) const noexcept -> bool = default;
};

/// unit of work over a subdomain
template <std::floating_point t_real_t, typename t_segment_t> struct interval_t
{
    using real_t = t_real_t;
    using segment_t = t_segment_t;

    using subdomain_t = subdomain_t<real_t>;
    using residual_t = residual_t<real_t>;

    subdomain_t subdomain;
    segment_t segment;
    segment_defects_t segment_defects;
    residual_t residual;

    constexpr auto operator==(interval_t const&) const noexcept -> bool = default;
};

/// orders by segment_defects, residual.weighted_error, then domain.left.x
struct interval_priority_less_t
{
    template <typename interval_t>
    constexpr auto operator()(interval_t const& lhs, interval_t const& rhs) const noexcept -> bool
    {
        using std::isfinite;
        assert(isfinite(lhs.residual.weighted_error));
        assert(isfinite(lhs.subdomain.left.x));
        assert(isfinite(rhs.residual.weighted_error));
        assert(isfinite(rhs.subdomain.left.x));

        auto const lhs_defects = lhs.segment_defects != segment_defects_t{0};
        auto const rhs_defects = rhs.segment_defects != segment_defects_t{0};

        // tie applies lexicographical compare
        return std::tie(lhs_defects, lhs.residual.weighted_error, lhs.subdomain.left.x)
            < std::tie(rhs_defects, rhs.residual.weighted_error, rhs.subdomain.left.x);
    }
};

/// constructs intervals from subdomains
template <typename t_interval_t, typename approximant_t, typename segment_factory_t, typename defect_analyzer_t,
    typename residual_estimator_t>
struct interval_factory_t
{
    using interval_t = t_interval_t;

    using real_t = interval_t::real_t;
    using subdomain_t = subdomain_t<real_t>;

    [[no_unique_address]] segment_factory_t create_segment;
    [[no_unique_address]] defect_analyzer_t analyze_defects;
    residual_estimator_t estimate_residual;

    constexpr auto create(auto const& sample_target_function, subdomain_t const& subdomain) const noexcept -> interval_t
    {
        using x_t = approximant_t::x_t;

        auto const x0 = to_fixed<x_t>(subdomain.left.x);
        auto const segment = create_segment(subdomain.left.y, subdomain.right.y, subdomain.log2_width);

        return {
            .subdomain = subdomain,
            .segment = segment,
            .segment_defects = analyze_defects(segment.coeffs()),
            .residual = estimate_residual(sample_target_function, approximant_t{.x0 = x0, .segment = segment},
                subdomain.left.x, subdomain.right.x),
        };
    }
};

} // namespace crv::spline
