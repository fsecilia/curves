// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "residual_estimator.hpp"
#include <crv/math/jet/jet.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

using real_t = float_t;
using jet_t = jet_t<real_t>;

constexpr auto max_node = 0.9;
struct fake_node_generator_t
{
    // to properly test the implementation uses max() correctly, the final node must not be the max node
    static constexpr auto nodes = std::array<real_t, 3>{0.1, max_node, 0.5};
    constexpr auto operator()() const noexcept { return nodes; }
};

struct uniform_norm_t
{
    constexpr auto operator()(jet_t target, jet_t approximation) const noexcept -> real_t
    {
        return primal(target) - primal(approximation);
    }
};

constexpr auto weight = 2.0;
struct linear_weight_function_t
{
    constexpr auto operator()(real_t node) const noexcept -> real_t { return node * weight; }
};

using sut_t = residual_estimator_t<real_t, fake_node_generator_t, uniform_norm_t, linear_weight_function_t>;
constexpr auto sut = sut_t{.generate_nodes = {}, .measure_error = {}, .apply_weight = {}};

struct target_function_sample_t
{
    jet_t y;
};

constexpr auto left = 3.0;
constexpr auto right = 10.0;
constexpr auto midpoint = (left + right) * 0.5;
constexpr auto interval_width = (right - left) * 0.5;

constexpr auto domain_node = left + (max_node * interval_width);
constexpr auto expected_max_scale = domain_node;

constexpr auto identifies_maximum_error()
{
    auto sample_target = [](real_t node) constexpr { return target_function_sample_t{jet_t{node, 0.0}}; };
    auto approximant = [](real_t node) constexpr { return jet_t{node * 0.5, 0.0}; };
    auto const expected_metric_error = domain_node - domain_node * 0.5;
    auto const expected_weight = midpoint * weight;

    auto const actual = sut(sample_target, approximant, left, right);

    return actual.scale == expected_max_scale && actual.metric_error == expected_metric_error
        && actual.weighted_error == actual.metric_error * expected_weight;
}
static_assert(identifies_maximum_error());

constexpr auto handles_negative_error()
{
    auto sample_target = [](real_t node) constexpr { return target_function_sample_t{jet_t{-node, 0.0}}; };
    auto approximant = [](real_t) constexpr { return jet_t{0.0, 0.0}; };

    auto const actual = sut(sample_target, approximant, left, right);

    return actual.scale == expected_max_scale && actual.metric_error == expected_max_scale;
}
static_assert(handles_negative_error());

} // namespace
} // namespace crv::spline
