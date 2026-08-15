// SPDX-License-Identifier: MIT

#include "local_coordinate.hpp"
#include <crv/math/fixed/fixed.hpp>
#include <crv/math/fixed/float_conversions.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/test/test.hpp>
#include <array>
#include <cmath>
#include <limits>

namespace crv::spline {
namespace {

using scalar_t = float_t;
constexpr auto convert = local_coordinate_converter_t<scalar_t>{};

static_assert(convert(cubic_t<scalar_t>{8.0, 4.0, 2.0, 1.0}, 2.0) == cubic_t<scalar_t>{1.0, 1.0, 1.0, 1.0});
static_assert(convert(cubic_t<scalar_t>{8.0, 4.0, 2.0, 1.0}, 0.5) == cubic_t<scalar_t>{64.0, 16.0, 4.0, 1.0});

TEST(spline_local_coordinate_test, preserves_polynomial_over_supported_width_range)
{
    using x_t = fixed_t<int64_t, 45>;

    auto const normalized = cubic_t<scalar_t>{1.4, -2.0, 1.0, 0.1};
    auto const widths = std::array{
        x_t::literal(1),
        x_t::literal(3),
        x_t{1} >> 10,
        to_fixed<x_t>(0.3),
        x_t{1},
        x_t{256},
    };
    auto const samples = std::array{0.0, 0.125, 1.0 / 3.0, 0.5, 0.875, 1.0};

    for (auto const width_fixed : widths)
    {
        auto const width = from_fixed<scalar_t>(width_fixed);
        auto const local = convert(normalized, width);

        for (auto const t : samples)
        {
            auto const u = t * width;
            auto const expected = normalized(t);
            auto const actual = local(u);
            auto const tolerance
                = std::max(std::abs(expected), scalar_t{1}) * std::numeric_limits<scalar_t>::epsilon() * scalar_t{32};
            EXPECT_NEAR(expected, actual, tolerance);
        }
    }
}

TEST(spline_local_coordinate_test, preserves_endpoint_derivative)
{
    auto const normalized = cubic_t<scalar_t>{1.4, -2.0, 1.0, 0.1};
    auto const width = scalar_t{0.3};
    auto const local = convert(normalized, width);

    auto const normalized_endpoint = normalized(jet_t<scalar_t>{1.0, 1.0 / width});
    auto const local_endpoint = local(jet_t<scalar_t>{width, 1.0});

    EXPECT_NEAR(primal(normalized_endpoint), primal(local_endpoint), 1e-14);
    EXPECT_NEAR(tangent(normalized_endpoint), tangent(local_endpoint), 1e-13);
}

} // namespace
} // namespace crv::spline
