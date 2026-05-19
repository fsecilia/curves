// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "residual.hpp"
#include <crv/math/jet/jet.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

using scalar_t = float_t;

namespace residual_estimator_tests {

constexpr auto max_node = 0.9;
struct fake_node_generator_t
{
    // to properly test the implementation uses max() correctly, the final node must not be the max node
    static constexpr auto nodes = std::array<scalar_t, 3>{0.1, max_node, 0.5};
    constexpr auto operator()() const noexcept { return nodes; }
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

constexpr auto handles_negative_error()
{
    auto sample_target = [](scalar_t node) constexpr { return target_function_sample_t{-node}; };
    auto approximant = [](scalar_t) constexpr { return scalar_t{0.0}; };

    auto const actual = sut(sample_target, approximant, left, midpoint, right);

    return actual.scale == expected_max_scale && actual.metric_error == expected_max_scale;
}
static_assert(handles_negative_error());

} // namespace residual_estimator_tests

// ====================================================================================================================
// approximant_t
// ====================================================================================================================

namespace approximant_tests {

struct spline_approximant_test_t : Test
{
    using scalar_t = float_t;
    using x_t = fixed_t<int16_t, 2>;
    using sut_t = approximant_t<scalar_t, x_t>;
};

TEST_F(spline_approximant_test_t, evaluates_exact_cubic_with_scale_and_offset)
{
    // P(t) = 1.0t^2 + 3.0t + 2.0
    auto const cubic = cubic_t{0.0, 1.0, 3.0, 2.0};

    auto const x0 = x_t{1};
    auto const log2_width = 1; // width = 2.0

    sut_t const sut{cubic, x0, log2_width};

    // input x = 2.0
    auto const input_x = scalar_t{2.0};

    // x_local = 2.0 - 1.0 = 1.0
    // t = x_local/2^1 = 0.5
    // y = P(0.5) = 1.0(0.5)^2 + 3.0(0.5) + 2.0 = 3.75

    auto const expected = scalar_t{3.75};

    auto const actual = sut(input_x);

    EXPECT_DOUBLE_EQ(expected, actual);
}

TEST_F(spline_approximant_test_t, quantizes_floating_point_input_to_fixed_grid)
{
    // P(t) = t
    auto const cubic = cubic_t{0.0, 0.0, 1.0, 0.0};
    auto const x0 = x_t{0};
    auto const log2_width = 0; // width = 1.0, t = x

    sut_t const sut{cubic, x0, log2_width};

    // input x that falls exactly between fixed-point representations
    auto const off_grid_input = scalar_t{0.125};

    // expected output should match what happens if we passed exactly 0.0
    auto const expected = scalar_t{0.0};

    auto const actual = sut(off_grid_input);

    EXPECT_DOUBLE_EQ(expected, actual);
}

} // namespace approximant_tests

} // namespace
} // namespace crv::spline
