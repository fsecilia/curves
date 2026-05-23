// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "subdomain_factory.hpp"
#include <crv/test/test.hpp>

namespace crv::spline::seed {
namespace {

using scalar_t = float_t;

struct vector_t
{
    scalar_t left;
    scalar_t expected_midpoint;
    scalar_t expected_right;
    int_t expected_log2_width;
};

struct spline_seed_subdomain_factory_test_t : TestWithParam<vector_t>
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

    using sut_t = subdomain_factory_t<x_t, subdomain_t>;

    static auto to_sample(scalar_t x) noexcept -> function_sample_t
    {
        return function_sample_t{.input = jet_t{x, 1.0}};
    }

    vector_t const& vector = GetParam();

    sut_t sut{};
};

TEST_P(spline_seed_subdomain_factory_test_t, calculates_subdomain_and_stride)
{
    auto const left_sample = to_sample(vector.left);
    auto const left = to_fixed<x_t>(vector.left);
    auto const right = to_fixed<x_t>(vector.expected_right);
    auto const stride = right - left;

    auto const actual = sut([](jet_t input) noexcept { return function_sample_t{input}; }, left_sample, left, stride);

    auto const expected = subdomain_t{
        .left = left_sample,
        .midpoint = to_sample(vector.expected_midpoint),
        .right = to_sample(vector.expected_right),
        .log2_width = vector.expected_log2_width,
    };
    EXPECT_EQ(expected, actual);
}

vector_t const vectors[] = {
    // zero start, integer stride
    {
        .left = 0.0,
        .expected_midpoint = 0.5,
        .expected_right = 1.0,
        .expected_log2_width = 0,
    },

    // fractional width (0.5 to 0.75)
    {
        .left = 0.5,
        .expected_midpoint = 0.625,
        .expected_right = 0.75,
        .expected_log2_width = -2,
    },

    // bisection floor; stride of 2 (0.0078125)
    {
        .left = 0.0,
        .expected_midpoint = 0.00390625,
        .expected_right = 0.0078125,
        .expected_log2_width = -7,
    },

    // large power of two
    {
        .left = 2.0,
        .expected_midpoint = 6.0,
        .expected_right = 10.0,
        .expected_log2_width = 3,
    },
};
INSTANTIATE_TEST_SUITE_P(edge_cases, spline_seed_subdomain_factory_test_t, ValuesIn(vectors));

} // namespace
} // namespace crv::spline::seed
