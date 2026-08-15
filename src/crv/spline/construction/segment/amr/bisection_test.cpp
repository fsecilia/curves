// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "bisection.hpp"
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/spline/construction/segment/amr/transfer_sampler.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

using scalar_t = float_t;
using fixed_x_t = fixed_t<int_t, 0>;
using sample_jet_t = crv::jet_t<scalar_t>;
using function_sample_t = spline::function_sample_t<sample_jet_t>;

struct subdomain_t
{
    using scalar_t = float_t;
    using x_t = fixed_x_t;
    using jet_t = sample_jet_t;

    x_t left_x;
    x_t midpoint_x;
    x_t right_x;
    function_sample_t left;
    function_sample_t midpoint;
    function_sample_t right;
};

constexpr auto sample_target_function = [](sample_jet_t const& jet) constexpr noexcept -> function_sample_t {
    return {.x = jet.f, .y = sample_jet_t{jet.f + 100.0, jet.df}};
};

constexpr auto make_sample(fixed_x_t x) noexcept -> function_sample_t
{
    return sample_target_function(sample_jet_t{from_fixed<scalar_t>(x), 1.0});
}

auto const parent = subdomain_t{
    .left_x = fixed_x_t{0},
    .midpoint_x = fixed_x_t{4},
    .right_x = fixed_x_t{9},
    .left = make_sample(fixed_x_t{0}),
    .midpoint = make_sample(fixed_x_t{4}),
    .right = make_sample(fixed_x_t{9}),
};

auto const bisector = bisector_t<bisection_t<subdomain_t>>{};

TEST(spline_bisection_test, bisects_at_exact_representable_midpoints)
{
    auto const result = bisector(sample_target_function, parent);

    EXPECT_EQ(result.left.left_x, fixed_x_t{0});
    EXPECT_EQ(result.left.midpoint_x, fixed_x_t{2});
    EXPECT_EQ(result.left.right_x, fixed_x_t{4});
    EXPECT_EQ(result.left.left, parent.left);
    EXPECT_EQ(result.left.right, parent.midpoint);

    EXPECT_EQ(result.right.left_x, fixed_x_t{4});
    EXPECT_EQ(result.right.midpoint_x, fixed_x_t{6});
    EXPECT_EQ(result.right.right_x, fixed_x_t{9});
    EXPECT_EQ(result.right.left, parent.midpoint);
    EXPECT_EQ(result.right.right, parent.right);
}

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG
TEST(spline_bisection_test, fails_loudly_without_distinct_representable_midpoint)
{
    auto const unsplittable = subdomain_t{
        .left_x = fixed_x_t{0},
        .midpoint_x = fixed_x_t{0},
        .right_x = fixed_x_t{1},
        .left = make_sample(fixed_x_t{0}),
        .midpoint = make_sample(fixed_x_t{0}),
        .right = make_sample(fixed_x_t{1}),
    };

    EXPECT_DEBUG_DEATH(bisector(sample_target_function, unsplittable), "distinct representable midpoint");
}
#endif

} // namespace
} // namespace crv::spline
