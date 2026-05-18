// SPDX-License-Identifier: MIT

/// \file
/// \brief amr units of work
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/polynomial.hpp>
#include <crv/math/spline/construction/function_sampler.hpp>
#include <crv/math/spline/construction/residual.hpp>
#include <numeric>

namespace crv::spline {

/// geometry of a refinement subdomain
///
/// This type brackets the subdomain [left, right]. It includes log2_width and samples for left, right, and midpoint.
template <std::floating_point t_scalar_t> struct subdomain_t
{
    using scalar_t = t_scalar_t;
    using function_sample_t = function_sample_t<scalar_t, jet_t<scalar_t>>;

    function_sample_t left;
    function_sample_t midpoint;
    function_sample_t right;
    int_t log2_width;

    constexpr auto operator==(subdomain_t const&) const noexcept -> bool = default;
};

/// unit of work over a subdomain
template <std::floating_point t_scalar_t> struct interval_t
{
    using scalar_t = t_scalar_t;

    using subdomain_t = subdomain_t<scalar_t>;
    using residual_t = residual_t<scalar_t>;
    using cubic_t = cubic_t<scalar_t>;

    subdomain_t subdomain;
    cubic_t cubic;
    residual_t residual;

    constexpr auto operator==(interval_t const&) const noexcept -> bool = default;
};

/// orders by residual.weighted_error then domain.left.x
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

        // tie applies lexicographical compare
        return std::tie(lhs.residual.weighted_error, lhs.subdomain.left.x)
            < std::tie(rhs.residual.weighted_error, rhs.subdomain.left.x);
    }
};

/// constructs intervals from subdomains
template <typename t_interval_t, typename approximant_t, typename hermite_converter_t, typename residual_estimator_t>
struct interval_factory_t
{
    using interval_t = t_interval_t;

    using scalar_t = interval_t::scalar_t;
    using subdomain_t = subdomain_t<scalar_t>;

    [[no_unique_address]] hermite_converter_t convert_hermite;
    residual_estimator_t estimate_residual;

    constexpr auto create(auto const& sample_target_function, subdomain_t const& subdomain) const noexcept -> interval_t
    {
        using x_t = approximant_t::x_t;

        auto const x0 = to_fixed<x_t>(subdomain.left.x);

        // convert from spline-global dy/dx to segment-local dy/dt via chain rule
        auto const dx_dt = std::ldexp(1.0, static_cast<int>(subdomain.log2_width));
        auto const local_left_y = jet_t{subdomain.left.y.f, subdomain.left.y.df * dx_dt};
        auto const local_right_y = jet_t{subdomain.right.y.f, subdomain.right.y.df * dx_dt};
        auto const cubic = convert_hermite(local_left_y, local_right_y);

        return {
            .subdomain = subdomain,
            .cubic = cubic,
            .residual = estimate_residual(sample_target_function,
                approximant_t{
                    .cubic = cubic,
                    .x0 = x0,
                    .log2_width = subdomain.log2_width,
                },
                subdomain.left.x, subdomain.midpoint.x, subdomain.right.x),
        };
    }
};

/// result of bisecting a subdomain
template <typename t_subdomain_t> struct bisection_t
{
    using subdomain_t = t_subdomain_t;

    subdomain_t left;
    subdomain_t right;
};

/// bisects subdomains
template <typename t_bisection_t> struct bisector_t
{
    using bisection_t = t_bisection_t;
    using subdomain_t = bisection_t::subdomain_t;

    constexpr auto operator()(auto const& sample_target_function, subdomain_t const& parent) const noexcept
        -> bisection_t
    {
        using scalar_t = subdomain_t::scalar_t;

        auto const child_log2_width = parent.log2_width - 1;
        return {
            .left = subdomain_t{
                .left = parent.left,
                .midpoint = sample_target_function(
                    jet_t<scalar_t>{std::midpoint(parent.left.x, parent.midpoint.x), scalar_t{1}}),
                .right = parent.midpoint,
                .log2_width = child_log2_width,
            },
            .right = subdomain_t{
                .left = parent.midpoint,
                .midpoint = sample_target_function(
                    jet_t<scalar_t>{std::midpoint(parent.midpoint.x, parent.right.x), scalar_t{1}}),
                .right = parent.right,
                .log2_width = child_log2_width,
            },
        };
    }
};

} // namespace crv::spline
