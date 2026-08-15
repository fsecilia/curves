// SPDX-License-Identifier: MIT

/// \file
/// \brief segment AMR unit of work over a subdomain
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/polynomial.hpp>
#include <crv/spline/construction/segment/amr/residual_estimator.hpp>
#include <crv/spline/construction/segment/amr/transfer_sample.hpp>
#include <crv/spline/construction/segment/local_coordinate.hpp>

namespace crv::spline {

/// geometry and samples of a refinement subdomain
///
/// Fixed-point positions are authoritative. Floating sample positions are derived from these exact positions when the
/// target is sampled.
template <std::floating_point t_scalar_t, is_fixed t_x_t> struct subdomain_t
{
    using scalar_t = t_scalar_t;
    using x_t = t_x_t;

    using jet_t = jet_t<scalar_t>;
    using function_sample_t = function_sample_t<jet_t>;

    x_t left_x;
    x_t midpoint_x;
    x_t right_x;
    function_sample_t left;
    function_sample_t midpoint;
    function_sample_t right;

    constexpr auto width() const noexcept -> x_t { return right_x - left_x; }

    constexpr auto operator==(subdomain_t const&) const noexcept -> bool = default;
};

/// unit of work over a subdomain
template <typename t_subdomain_t, typename t_cubic_t, typename t_segment_t> struct interval_t
{
    using subdomain_t = t_subdomain_t;
    using cubic_t = t_cubic_t;
    using segment_t = t_segment_t;

    using scalar_t = subdomain_t::scalar_t;
    using residual_t = residual_t<scalar_t>;

    cubic_t cubic; // local-u polynomial
    segment_t segment;
    subdomain_t subdomain;
    residual_t residual;

    constexpr auto operator==(interval_t const&) const noexcept -> bool = default;
};

/// orders by residual.weighted_error then exact domain.left_x
struct interval_priority_less_t
{
    template <typename interval_t>
    constexpr auto operator()(interval_t const& lhs, interval_t const& rhs) const noexcept -> bool
    {
        using std::isfinite;
        assert(isfinite(lhs.residual.weighted_error));
        assert(isfinite(rhs.residual.weighted_error));

        return std::tie(lhs.residual.weighted_error, lhs.subdomain.left_x)
            < std::tie(rhs.residual.weighted_error, rhs.subdomain.left_x);
    }
};

/// constructs intervals from subdomains
template <typename t_interval_t, typename segment_factory_t, typename approximant_factory_t,
    typename hermite_converter_t, typename local_coordinate_converter_t, typename residual_estimator_t>
struct interval_factory_t
{
    using interval_t = t_interval_t;

    using scalar_t = interval_t::scalar_t;
    using approximant_t = approximant_factory_t::approximant_t;
    using x_t = approximant_t::x_t;
    using subdomain_t = typename interval_t::subdomain_t;

    [[no_unique_address]] segment_factory_t segment_factory;
    [[no_unique_address]] approximant_factory_t approximant_factory;
    [[no_unique_address]] hermite_converter_t convert_hermite;
    [[no_unique_address]] local_coordinate_converter_t convert_local_coordinate;
    residual_estimator_t estimate_residual;

    constexpr auto operator()(auto const& target, subdomain_t const& subdomain) const noexcept -> interval_t
    {
        auto const width_fixed = subdomain.width();
        assert(width_fixed > x_t{0});
        auto const width = from_fixed<scalar_t>(width_fixed);

        // convert transfer-space dT/dx to normalized Hermite dT/dt via dx/dt = width
        auto const local_left_y = jet_t{subdomain.left.y.f, subdomain.left.y.df * width};
        auto const local_right_y = jet_t{subdomain.right.y.f, subdomain.right.y.df * width};

        auto const normalized_cubic = convert_hermite(local_left_y, local_right_y);
        auto const cubic = convert_local_coordinate(normalized_cubic, width);
        auto const segment = segment_factory(cubic, width_fixed, subdomain.left_x);

        auto const left = from_fixed<scalar_t>(subdomain.left_x);
        auto const midpoint = from_fixed<scalar_t>(subdomain.midpoint_x);
        auto const right = from_fixed<scalar_t>(subdomain.right_x);

        return {
            .cubic = cubic,
            .segment = segment,
            .subdomain = subdomain,
            .residual
            = estimate_residual(target, approximant_factory(segment, subdomain.left_x), left, midpoint, right),
        };
    }
};

} // namespace crv::spline
