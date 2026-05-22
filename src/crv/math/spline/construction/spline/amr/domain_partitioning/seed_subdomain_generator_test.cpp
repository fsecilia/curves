// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "seed_subdomain_generator.hpp"
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

using scalar_t = float_t;

struct vector_t
{
    scalar_t current_x;
    scalar_t target_x;
    scalar_t expected_midpoint_x;
    scalar_t expected_next_x;
    int_t expected_log2_width;
};

struct spline_seed_subdomain_generator_test_t : TestWithParam<vector_t>
{
    using x_t = fixed_t<int_t, 8>;
    using jet_t = jet_t<scalar_t>;

    struct function_sample_t
    {
        jet_t input;

        constexpr auto operator==(function_sample_t const&) const noexcept -> bool = default;
    };

    struct subdomain_t
    {
        using scalar_t = scalar_t;
        using jet_t = jet_t;
        using function_sample_t = function_sample_t;

        function_sample_t left;
        function_sample_t midpoint;
        function_sample_t right;
        int_t log2_width;

        constexpr auto operator==(subdomain_t const&) const noexcept -> bool = default;
    };

    struct stride_calculator_t
    {
        using x_t = x_t;

        constexpr auto operator()(x_t const& current_x, x_t const& target_x) const noexcept -> x_t
        {
            return target_x - current_x;
        }
    };

    using sut_t = seed_subdomain_generator_t<subdomain_t, stride_calculator_t>;
    using result_t = sut_t::result_t;

    static auto to_sample(scalar_t x) noexcept -> function_sample_t
    {
        return function_sample_t{.input = jet_t{x, 1.0}};
    }

    vector_t const& vector = GetParam();

    sut_t sut{};
};

TEST_P(spline_seed_subdomain_generator_test_t, calculates_subdomain_and_stride)
{
    auto const current_x_fixed = to_fixed<x_t>(vector.current_x);
    auto const target_x_fixed = to_fixed<x_t>(vector.target_x);
    auto const left_sample = to_sample(vector.current_x);

    auto const actual = sut(
        [](jet_t input) noexcept { return function_sample_t{input}; }, left_sample, current_x_fixed, target_x_fixed);

    auto const expected = sut_t::result_t{
        .subdomain = {
            .left = to_sample(vector.current_x),
            .midpoint = to_sample(vector.expected_midpoint_x),
            .right = to_sample(vector.expected_next_x),
            .log2_width = vector.expected_log2_width,
        },
        .next_x = to_fixed<x_t>(vector.expected_next_x),
    };
    EXPECT_EQ(expected, actual);
}

vector_t const vectors[] = {

    // zero start, integer stride
    {
        .current_x = 0.0,
        .target_x = 1.0,
        .expected_midpoint_x = 0.5,
        .expected_next_x = 1.0,
        .expected_log2_width = 0,
    },

    // purely fractional domain (0.5 to 0.75)
    {
        .current_x = 0.5,
        .target_x = 0.75,
        .expected_midpoint_x = 0.625,
        .expected_next_x = 0.75,
        .expected_log2_width = -2,
    },

    // bisection floor: stride value of 2 (0.0078125)
    {
        .current_x = 0.0,
        .target_x = 0.0078125,
        .expected_midpoint_x = 0.00390625,
        .expected_next_x = 0.0078125,
        .expected_log2_width = -7,
    },

    // large power of two
    {
        .current_x = 2.0,
        .target_x = 10.0,
        .expected_midpoint_x = 6.0,
        .expected_next_x = 10.0,
        .expected_log2_width = 3,
    },
};
INSTANTIATE_TEST_SUITE_P(edge_cases, spline_seed_subdomain_generator_test_t, ValuesIn(vectors));

} // namespace
} // namespace crv::spline
