// SPDX-License-Identifier: MIT

/// \file
/// \brief estimates gain error between target and fixed approximation
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/algorithm.hpp>
#include <crv/math/abs.hpp>
#include <cassert>

namespace crv::spline {

/// max gain scale and error between target gain and final fixed approximant over a subdomain
template <std::floating_point scalar_t> struct residual_t
{
    scalar_t metric_error; // error measured using a metric
    scalar_t weighted_error; // metric_error weighted perceptually
    scalar_t scale; // absolute magnitude of primal

    constexpr auto operator==(residual_t const&) const noexcept -> bool = default;
};

/// estimates worst sampled gain error over a subdomain
///
/// Samples come from node_generator_t, error_metric_t measures each target/approximant gap, and the largest error is
/// scaled by the domain weight.
///
/// \pre node_generator_t yields standard nodes in (0, 1)
/// \pre error_metric_t assigns only nonnegative values
template <std::floating_point scalar_t, typename node_generator_t, typename error_metric_t, typename weight_function_t>
struct residual_estimator_t
{
    using residual_t = residual_t<scalar_t>;

    [[no_unique_address]] node_generator_t generate_nodes;
    error_metric_t measure_error;
    weight_function_t apply_weight;

    constexpr auto operator()(auto const& target, auto const& approximant, scalar_t left, scalar_t midpoint,
        scalar_t right) const noexcept -> residual_t
    {
        auto const interval_width = right - left;

        // sample nodes and track largest scale/error
        auto max_residual = residual_t{};
        for (auto const standard_node : generate_nodes())
        {
            assert(scalar_t{0} < standard_node && standard_node < scalar_t{1} && "nodes must be in (0, 1)");

            // map standard node to this subdomain
            auto const domain_node = left + standard_node * interval_width;

            // measure target/approximant gain error
            auto const approximation = approximant(domain_node);
            auto const target_gain = target.gain(domain_node);
            auto const metric_error = measure_error(target_gain, approximation);

            assert(metric_error >= scalar_t{0} && "metrics must assign nonnegative values");

            // keep maxima
            max_residual.scale = max(max_residual.scale, abs(target_gain));
            max_residual.metric_error = max(max_residual.metric_error, metric_error);
        }

        auto const weight = apply_weight(midpoint);
        max_residual.weighted_error = max_residual.metric_error * weight;
        return max_residual;
    }
};

} // namespace crv::spline
