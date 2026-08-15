// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include <crv/spline/pipeline_config.hpp>
#include <crv/spline/spline_factory.hpp>
#include <crv/spline/spline_factory_policy.hpp>
#include <crv/test/test.hpp>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdfloat>
#include <vector>

namespace crv::spline {
namespace {

TEST(spline_factory_integration_test, evaluates_spline_within_tolerance)
{
#if 0
    using scalar_t = std::float128_t;
#else
    using scalar_t = float_t;
#endif

    using policy_t = default_spline_policy_t<scalar_t, prod_pipeline_config_t>;
    using x_t = policy_t::x_t;

    constexpr auto global_tolerance = scalar_t{1e-10};
    constexpr auto domain_end = policy_t::domain_end;

    auto const target_function = [](auto x) static noexcept -> decltype(x) {
        using std::log1p;
        return 2.1 * log1p(x);
    };

    using spline_factory_t = spline_factory_t<policy_t, spline_generator_factory_t<policy_t>>;
    using spline_t = spline_factory_t::spline_t;
    auto spline = spline_t{};

    auto const arbitrary_knot = to_fixed<x_t>(7.123456789);
    auto const adjacent_knot = x_t::literal(arbitrary_knot.value + 1);
    auto critical_points = std::vector{x_t{1 << 3}, x_t{1 << 5}, arbitrary_knot, adjacent_knot, to_fixed<x_t>(248.973)};
    spline_factory_t{}(spline, target_function, global_tolerance, critical_points);

    // test knots closer than min_segment_width
    //
    // Supplied knots are exact geometry. In particular, the arbitrary knot is not snapped to the a min_width grid, and
    // an adjacent raw fixed-point knot remains a distinct segment boundary even though it is far below min_width.
    EXPECT_EQ(spline.payload.segment_locator.locate(arbitrary_knot).origin, arbitrary_knot);
    EXPECT_EQ(spline.payload.segment_locator.locate(adjacent_knot).origin, adjacent_knot);

    auto x_fixed = x_t{0};
    auto const sample_count = 255;
    auto const dx = x_t{domain_end} / sample_count;

    for (auto sample = 0; sample < sample_count; ++sample, x_fixed += dx)
    {
        auto const x_real = from_fixed<scalar_t>(x_fixed);

        auto const expected_y = target_function(x_real);
        auto const actual_y = from_fixed<scalar_t>(spline(x_fixed));

        auto const difference = actual_y - expected_y;

        ASSERT_LT(abs(difference), 8.0e-7);

        std::cout << std::setprecision(4) << "x = " << static_cast<float_max_t>(x_real)
                  << ", f(x) = " << static_cast<float_max_t>(expected_y)
                  << ", y_actual = " << static_cast<float_max_t>(actual_y)
                  << ", Δy = " << static_cast<float_max_t>(difference) << std::endl;
    }
    std::cout << std::endl;
}

} // namespace
} // namespace crv::spline
