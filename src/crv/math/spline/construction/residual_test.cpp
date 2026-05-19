// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "residual.hpp"
#include <crv/math/jet/jet.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

using scalar_t = float_t;

constexpr auto max_node = 0.9;
struct fake_node_generator_t
{
    // to properly test the implementation uses max() correctly, the final node must not be the max node
    using nodes_t = std::array<scalar_t, 3>;
    static constexpr auto nodes = nodes_t{0.1, max_node, 0.5};
    constexpr auto operator()() const noexcept -> nodes_t const& { return nodes; }
};

struct uniform_norm_t
{
    constexpr auto operator()(scalar_t target, scalar_t approximation) const noexcept -> scalar_t
    {
        return target - approximation;
    }
};

constexpr auto weight = 2.0;
struct linear_weight_function_t
{
    constexpr auto operator()(scalar_t node) const noexcept -> scalar_t { return node * weight; }
};

using sut_t = residual_estimator_t<scalar_t, fake_node_generator_t, uniform_norm_t, linear_weight_function_t>;
constexpr auto sut = sut_t{.generate_nodes = {}, .measure_error = {}, .apply_weight = {}};

struct target_function_sample_t
{
    scalar_t y;
};

constexpr auto left = 3.0;
constexpr auto right = 10.0;
constexpr auto midpoint = (left + right) * 0.5;
constexpr auto interval_width = right - left;

constexpr auto domain_node = left + (max_node * interval_width);
constexpr auto expected_max_scale = domain_node;

constexpr auto identifies_maximum_error()
{
    static constexpr auto target_scale = 1.1;
    static constexpr auto approximant_scale = 0.9;

    auto sample_target = [](scalar_t node) constexpr { return target_function_sample_t{node * target_scale}; };
    auto approximant = [](scalar_t node) constexpr { return scalar_t{node * approximant_scale}; };
    auto const expected_metric_error = domain_node * target_scale - domain_node * approximant_scale;
    auto const expected_weight = midpoint * weight;

    auto const actual = sut(sample_target, approximant, left, midpoint, right);

    return actual.scale == expected_max_scale * target_scale && actual.metric_error == expected_metric_error
        && actual.weighted_error == actual.metric_error * expected_weight;
}
static_assert(identifies_maximum_error());

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

// metrics, by definition, must assign a nonnegative value, so sut asserts
TEST(spline_residual_estimator, metrics_must_be_positive)
{
    auto sample_target = [](scalar_t node) constexpr { return target_function_sample_t{-node}; };
    auto approximant = [](scalar_t) constexpr { return scalar_t{0.0}; };

    EXPECT_DEBUG_DEATH(sut(sample_target, approximant, left, midpoint, right), "nonnegative");
}

// nodes must be in (0, 1)
//
// Standard nodes must be in [0, 1], but for a cubic spline generated from a hermite basis, the spline always goes
// through the knots, so measuring at segment endpoints is not effective.
struct spline_residual_estimator_test_node_endpoints_t : Test
{
    struct single_point_node_generator_t
    {
        using nodes_t = std::vector<scalar_t>;

        scalar_t node = 0.0;

        constexpr auto operator()() const noexcept -> nodes_t { return {node}; }
    };

    static constexpr auto sample_target = [](scalar_t node) constexpr { return target_function_sample_t{node}; };
    static constexpr auto approximant = [](scalar_t) constexpr { return scalar_t{0.0}; };

    using sut_t
        = residual_estimator_t<scalar_t, single_point_node_generator_t, uniform_norm_t, linear_weight_function_t>;
};

TEST_F(spline_residual_estimator_test_node_endpoints_t, nodes_excludes_left_endpoint)
{
    constexpr auto sut = sut_t{.generate_nodes = {}, .measure_error = {}, .apply_weight = {}};
    EXPECT_DEBUG_DEATH(sut(sample_target, approximant, left, midpoint, right), "in \\(0, 1\\)");
}

TEST_F(spline_residual_estimator_test_node_endpoints_t, nodes_excludes_right_endpoint)
{
    constexpr auto sut = sut_t{.generate_nodes = {.node = 1.0}, .measure_error = {}, .apply_weight = {}};
    EXPECT_DEBUG_DEATH(sut(sample_target, approximant, left, midpoint, right), "in \\(0, 1\\)");
}

#endif

} // namespace
} // namespace crv::spline
