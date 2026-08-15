// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/lib.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/polynomial.hpp>
#include <crv/spline/pipeline_config.hpp>
#include <crv/spline/spline_factory_policy.hpp>
#include <crv/test/test.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <type_traits>
#include <vector>

namespace crv::spline {
namespace {

struct spline_induced_gain_segment_test_t : Test
{
    using scalar_t = float_t;
    using policy_t = default_spline_policy_t<scalar_t, prod_pipeline_config_t>;
    using x_t = policy_t::x_t;
    using y_t = policy_t::y_t;
    using cubic_t = policy_t::cubic_t;
    using segment_t = policy_t::segment_t;
    using segment_factory_t = policy_t::segment_factory_t;

    segment_factory_t make_segment;

    auto test(cubic_t const& transfer, x_t x0, x_t width, std::vector<x_t> const& offsets, scalar_t tolerance = 3e-10)
        -> void
    {
        ASSERT_GE(x0, x_t{0});
        ASSERT_GT(width, x_t{0});
        auto const segment = make_segment(transfer, width, x0);

        for (auto const u : offsets)
        {
            ASSERT_GE(u, x_t{0});
            ASSERT_LE(u, width);
            auto const x = x_t::literal(x0.value + u.value);
            auto const x_real = from_fixed<scalar_t>(x);
            auto const u_real = from_fixed<scalar_t>(u);
            auto const expected = x == x_t{0} ? transfer[2] : transfer(u_real) / x_real;
            auto const actual = from_fixed<scalar_t>(segment(x, x0));

            EXPECT_NEAR(actual, expected, tolerance * std::max(std::abs(expected), scalar_t{1}))
                << "x0=" << from_fixed<scalar_t>(x0) << ", u=" << u_real << ", x=" << x_real;
        }
    }
};

static_assert(sizeof(typename spline_induced_gain_segment_test_t::segment_t) == 32);
static_assert(alignof(typename spline_induced_gain_segment_test_t::segment_t) == 32);
static_assert(std::is_trivially_copyable_v<typename spline_induced_gain_segment_test_t::segment_t>);

TEST_F(spline_induced_gain_segment_test_t, first_segment_is_continuous_at_zero_without_division)
{
    // T(u)=0.75u + 0.5u^2 - 0.125u^3, so G(0)=T'(0)=0.75.
    auto const transfer = cubic_t{-0.125, 0.5, 0.75, 0.0};
    auto const width = to_fixed<x_t>(0.3);
    test(transfer, x_t{0}, width,
        {x_t{0}, x_t::literal(1), x_t::literal(width.value / 3), x_t::literal(width.value - 1), width});
}

TEST_F(spline_induced_gain_segment_test_t, agrees_with_transfer_quotient_on_arbitrary_non_dyadic_interval)
{
    auto const transfer = cubic_t{0.03125, -0.125, 1.25, 3.75};
    auto const x0 = to_fixed<x_t>(7.123456789);
    auto const width = to_fixed<x_t>(0.300000001);
    test(transfer, x0, width,
        {x_t{0}, x_t::literal(1), x_t::literal(width.value / 2), x_t::literal(width.value - 1), width});
}

TEST_F(spline_induced_gain_segment_test_t, agrees_with_transfer_quotient_for_odd_raw_origin_and_width)
{
    auto const x0 = x_t::literal(123456789);
    auto const width = x_t::literal(12345);
    auto const transfer = cubic_t{-0.25, 0.125, 2.0, from_fixed<scalar_t>(x0) * 1.75};
    test(transfer, x0, width, {x_t{0}, x_t::literal(1), x_t::literal(6172), x_t::literal(12344), width});
}

TEST_F(spline_induced_gain_segment_test_t, agrees_near_minimum_refinement_width_and_on_large_width)
{
    auto const small_width = to_fixed<x_t>(std::ldexp(1.0, policy_t::log2_min_width));
    auto const small_x0 = x_t::literal(small_width.value * 3 + 7);
    auto const small_transfer = cubic_t{0.5, -0.25, 1.125, from_fixed<scalar_t>(small_x0) * 1.125};
    test(small_transfer, small_x0, small_width,
        {x_t{0}, x_t::literal(1), x_t::literal(small_width.value / 2), x_t::literal(small_width.value - 1),
            small_width});

    auto const large_x0 = x_t{17};
    auto const large_width = x_t{200};
    auto const large_transfer = cubic_t{1e-7, -2e-4, 1.5, 34.0};
    test(large_transfer, large_x0, large_width,
        {x_t{0}, x_t::literal(1), large_width / 2, x_t::literal(large_width.value - 1), large_width}, 2e-9);
}

TEST_F(spline_induced_gain_segment_test_t, handles_small_and_large_g0_minus_s_without_a_second_approximation)
{
    auto const width = to_fixed<x_t>(0.75);
    auto const x0 = x_t{2};

    // g0=a/x0=2, close to S near the left endpoint.
    test(cubic_t{1e-4, -2e-4, 2.0001, 4.0}, x0, width, {x_t{0}, x_t::literal(1), width / 2, width});

    // g0=20 while S starts near one, exercising a much larger correction.
    test(cubic_t{-0.01, 0.05, 1.0, 40.0}, x0, width, {x_t{0}, x_t::literal(1), width / 2, width});
}

} // namespace
} // namespace crv::spline
