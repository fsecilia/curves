// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "subdomain_factory.hpp"
#include <crv/spline/construction/segment/amr/interval.hpp>
#include <crv/test/test.hpp>

namespace crv::spline::seed {
namespace {

using scalar_t = float_t;
using x_t = fixed_t<int_t, 8>;
using subdomain_t = spline::subdomain_t<scalar_t, x_t>;
using jet_t = subdomain_t::jet_t;
using function_sample_t = subdomain_t::function_sample_t;
using sut_t = subdomain_factory_t<x_t, subdomain_t>;

constexpr auto sample = [](jet_t input) noexcept -> function_sample_t { return {.x = input.f, .y = input}; };

TEST(spline_seed_subdomain_factory_test, preserves_exact_non_dyadic_endpoints)
{
    auto const left = x_t::literal(100);
    auto const right = x_t::literal(105); // odd raw width
    auto const left_sample = sample(jet_t{from_fixed<scalar_t>(left), 1.0});

    auto const actual = sut_t{}(sample, left_sample, left, right);

    EXPECT_EQ(actual.left_x, left);
    EXPECT_EQ(actual.midpoint_x, x_t::literal(102));
    EXPECT_EQ(actual.right_x, right);
    EXPECT_EQ(actual.left, left_sample);
    EXPECT_EQ(actual.midpoint.x, from_fixed<scalar_t>(actual.midpoint_x));
    EXPECT_EQ(actual.right.x, from_fixed<scalar_t>(right));
}

TEST(spline_seed_subdomain_factory_test, permits_adjacent_representable_endpoints)
{
    auto const left = x_t::literal(100);
    auto const right = x_t::literal(101);
    auto const left_sample = sample(jet_t{from_fixed<scalar_t>(left), 1.0});

    auto const actual = sut_t{}(sample, left_sample, left, right);

    EXPECT_EQ(actual.left_x, left);
    EXPECT_EQ(actual.midpoint_x, left);
    EXPECT_EQ(actual.right_x, right);
}

} // namespace
} // namespace crv::spline::seed
