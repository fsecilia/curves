// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "bisection.hpp"
#include <crv/math/jet/jet.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

using scalar_t = float_t;
using jet_t = jet_t<scalar_t>;

struct function_sample_t
{
    scalar_t x;
    jet_t y;

    constexpr auto operator==(function_sample_t const&) const noexcept -> bool = default;
};

constexpr auto sample_target_function
    = [](auto const& jet) constexpr noexcept { return function_sample_t{.x = jet.f, .y = jet.f + 100.0}; };

struct subdomain_t
{
    using scalar_t = scalar_t;
    using jet_t = jet_t;

    function_sample_t left;
    function_sample_t midpoint;
    function_sample_t right;
    int_t log2_width;

    constexpr auto operator==(subdomain_t const&) const noexcept -> bool = default;
};

constexpr auto parent = subdomain_t{
    .left = {.x = 0.0, .y = 0.0}, .midpoint = {.x = 4.0, .y = 0.0}, .right = {.x = 8.0, .y = 0.0}, .log2_width = 4};

constexpr auto bisector = bisector_t<bisection_t<subdomain_t>>{};
constexpr auto result = bisector(sample_target_function, parent);

//
// left child
//

// topology
static_assert(result.left.left == parent.left);
static_assert(result.left.right == parent.midpoint);

// midpoint
static_assert(result.left.midpoint.x == 2.0);
static_assert(result.left.midpoint.y == 102.0);

// width
static_assert(result.left.log2_width == 3);

//
// right child
//

// topology
static_assert(result.right.left == parent.midpoint);
static_assert(result.right.right == parent.right);

// midpoint evaluation
static_assert(result.right.midpoint.x == 6.0);
static_assert(result.right.midpoint.y == 106.0);

// width
static_assert(result.right.log2_width == 3);

} // namespace
} // namespace crv::spline
