// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "approximant.hpp"
#include <crv/math/fixed/fixed.hpp>
#include <crv/test/test.hpp>

namespace crv::spline {
namespace {

// --------------------------------------------------------------------------------------------------------------------
// approximant_t
// --------------------------------------------------------------------------------------------------------------------

struct spline_approximant_test_t : Test
{
    using scalar_t = float_t;
    using x_t = fixed_t<int16_t, 2>;
    using y_t = fixed_t<int32_t, 4>;

    struct segment_t
    {
        using x_t = x_t;
        constexpr auto operator()(x_t x) const noexcept -> y_t { return y_t::convert(x * 7); }
    };

    using sut_t = approximant_t<scalar_t, segment_t>;

    auto test(scalar_t x0_float, scalar_t x_float, scalar_t y_expected) const noexcept -> void
    {
        auto const x0_fixed = to_fixed<x_t>(x0_float);
        sut_t const sut{segment_t{}, x0_fixed};

        auto const y_actual = sut(x_float);

        EXPECT_DOUBLE_EQ(y_expected, y_actual);
    }
};

TEST_F(spline_approximant_test_t, evaluates_exact_cubic_with_scale_and_offset)
{
    test(2.0, 5.0, (5.0 - 2.0) * 7.0);
}

TEST_F(spline_approximant_test_t, quantizes_floating_point_input_to_fixed_grid)
{
    // 2.1 should quantize to 2.0
    test(2.1, 5.0, (5.0 - 2.0) * 7.0);
}

// --------------------------------------------------------------------------------------------------------------------
// approximant_factory_t
// --------------------------------------------------------------------------------------------------------------------

namespace approximant_factory_tests {

struct approximant_t
{
    using segment_t = int_t;
    using x_t = int_t;

    segment_t segment;
    x_t x0;

    constexpr auto operator==(approximant_t const&) const noexcept -> bool = default;
};
using sut_t = approximant_factory_t<approximant_t>;
constexpr auto sut = sut_t{};

constexpr auto segment = 5;
constexpr auto x0 = 7;

static_assert(approximant_t{.segment = segment, .x0 = x0} == sut(segment, x0));

} // namespace approximant_factory_tests

} // namespace
} // namespace crv::spline
