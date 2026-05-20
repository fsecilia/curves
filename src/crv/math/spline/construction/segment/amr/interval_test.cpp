// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "interval.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::spline {
namespace {

using scalar_t = float_t;
using jet_t = jet_t<scalar_t>;

// --------------------------------------------------------------------------------------------------------------------
// interval_priority_less_t
// --------------------------------------------------------------------------------------------------------------------

namespace interval_priority_less_tests {

using scalar_t = float_t;

struct subdomain_t
{
    using scalar_t = scalar_t;

    struct function_sample_t
    {
        scalar_t x;
    };
    function_sample_t left;

    struct residual_t
    {
        scalar_t weighted_error;
    };
    residual_t residual;
};

using segment_t = int_t;
using cubic_t = int_t;

using sut_t = interval_t<subdomain_t, cubic_t, segment_t>;

constexpr auto construct_sut(scalar_t weighted_error, scalar_t left_x) noexcept -> sut_t
{
    auto result = sut_t{};
    result.subdomain.left.x = left_x;
    result.residual.weighted_error = weighted_error;
    return result;
}

constexpr auto sut = interval_priority_less_t{};

// weighted error dominates left.x
static_assert(sut(construct_sut(0.0, 1e30), construct_sut(1.0, 0.0)));
static_assert(sut(construct_sut(0.0, 1e30), construct_sut(1.0, 0.0)));

// left.x breaks ties
static_assert(sut(construct_sut(5.0, 1.0), construct_sut(5.0, 2.0)));
static_assert(!sut(construct_sut(5.0, 2.0), construct_sut(5.0, 1.0)));

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

namespace death_tests {

constexpr auto nan = std::numeric_limits<scalar_t>::quiet_NaN();
constexpr auto inf = std::numeric_limits<scalar_t>::infinity();

TEST(spline_interval_priority_less, death_by_lhs_residual_weighted_error)
{
    EXPECT_DEBUG_DEATH(sut(construct_sut(nan, 0.0), construct_sut(0.0, 0.0)), "isfinite");
}

TEST(spline_interval_priority_less, death_by_lhs_left_x)
{
    EXPECT_DEBUG_DEATH(sut(construct_sut(0.0, inf), construct_sut(0.0, 0.0)), "isfinite");
}

TEST(spline_interval_priority_less, death_by_rhs_residual_weighted_error)
{
    EXPECT_DEBUG_DEATH(sut(construct_sut(0.0, 0.0), construct_sut(-inf, 0.0)), "isfinite");
}

TEST(spline_interval_priority_less, death_by_rhs_left_x)
{
    EXPECT_DEBUG_DEATH(sut(construct_sut(0.0, 0.0), construct_sut(0.0, nan)), "isfinite");
}

} // namespace death_tests

#endif // #if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

} // namespace interval_priority_less_tests

// --------------------------------------------------------------------------------------------------------------------
// interval_factory_t
// --------------------------------------------------------------------------------------------------------------------

namespace interval_factory_tests {

struct spline_interval_factory_test_t : Test
{
    using subdomain_t = subdomain_t<scalar_t>;
    using function_sample_t = function_sample_t<jet_t>;

    using cubic_t = cubic_t<scalar_t>;
    using x_t = fixed_t<int64_t, 0>;

    struct segment_t
    {
        cubic_t cubic;
        int_t log2_width;

        constexpr auto operator==(segment_t const&) const noexcept -> bool = default;
    };

    struct segment_factory_t
    {
        using segment_t = segment_t;

        constexpr auto operator()(cubic_t const& cubic, int_t log2_width) const noexcept -> segment_t
        {
            return {cubic, log2_width};
        }
    };

    struct approximant_t
    {
        using x_t = spline_interval_factory_test_t::x_t;

        segment_t segment;
        x_t x0;

        constexpr auto operator==(approximant_t const&) const noexcept -> bool = default;
    };

    struct approximant_factory_t
    {
        using approximant_t = approximant_t;

        constexpr auto operator()(segment_t const& segment, x_t x0) const noexcept -> approximant_t
        {
            return approximant_t{segment, x0};
        }
    };

    struct mock_hermite_converter_t
    {
        virtual ~mock_hermite_converter_t() = default;
        MOCK_METHOD(cubic_t, call, (jet_t left, jet_t right), (const, noexcept));
    };
    StrictMock<mock_hermite_converter_t> mock_hermite_converter;

    struct hermite_converter_t
    {
        mock_hermite_converter_t* mock = nullptr;
        auto operator()(jet_t left_y, jet_t right_y) const noexcept -> cubic_t { return mock->call(left_y, right_y); }
    };

    struct residual_t
    {
        int_t id = 0;
        auto operator==(residual_t const&) const noexcept -> bool = default;
    };

    struct sample_target_function_t
    {
        int_t id = 0;
        constexpr auto operator==(sample_target_function_t const&) const noexcept -> bool = default;
    };

    struct mock_residual_estimator_t
    {
        virtual ~mock_residual_estimator_t() = default;
        MOCK_METHOD(residual_t, call,
            (sample_target_function_t sample_target_function, approximant_t approximant, scalar_t left_x,
                scalar_t midpoint_x, scalar_t right_x),
            (const, noexcept));
    };
    StrictMock<mock_residual_estimator_t> mock_residual_estimator;

    struct residual_estimator_t
    {
        mock_residual_estimator_t* mock = nullptr;
        auto operator()(sample_target_function_t sample_target_function, approximant_t approximant, scalar_t left_x,
            scalar_t midpoint_x, scalar_t right_x) const noexcept -> residual_t
        {
            return mock->call(sample_target_function, approximant, left_x, midpoint_x, right_x);
        }
    };

    struct interval_t
    {
        using scalar_t = scalar_t;

        cubic_t cubic;
        segment_t segment;
        subdomain_t subdomain;
        residual_t residual;

        constexpr auto operator==(interval_t const&) const noexcept -> bool = default;
    };

    using sut_t = interval_factory_t<interval_t, segment_factory_t, approximant_factory_t, hermite_converter_t,
        residual_estimator_t>;
    sut_t sut{
        .segment_factory = {},
        .approximant_factory = {},
        .convert_hermite = hermite_converter_t{&mock_hermite_converter},
        .estimate_residual = residual_estimator_t{&mock_residual_estimator},
    };

    sample_target_function_t const sample_target_function{1};
    function_sample_t const left{.x = 2.0, .y = {3.0, 4.0}};
    function_sample_t const midpoint{.x = 5.0, .y = {6.0, 7.0}};
    function_sample_t const right{.x = 8.0, .y = {9.0, 10.0}};
    int_t log2_width = 11;
    cubic_t const cubic{1.0, 2.0, 3.0, 4.0};
    segment_t segment = segment_t{.cubic = cubic, .log2_width = log2_width};
    residual_t const residual{14};

    x_t x0 = to_fixed<x_t>(left.x);

    interval_t const expected
    {
        .cubic = cubic,
        .segment = segment,
        .subdomain = subdomain_t{
            .left = left,
            .midpoint = midpoint,
            .right = right,
            .log2_width = log2_width,
        },
        .residual = residual,
    };
};

TEST_F(spline_interval_factory_test_t, call)
{
    // sut applies chain rule locally from dy/dx to dy/dt.
    auto const dx_dt = static_cast<scalar_t>(1 << log2_width);
    auto local_left_y = left.y;
    auto local_right_y = right.y;
    local_left_y.df *= dx_dt;
    local_right_y.df *= dx_dt;
    EXPECT_CALL(mock_hermite_converter, call(local_left_y, local_right_y)).WillOnce(Return(cubic));

    EXPECT_CALL(mock_residual_estimator,
        call(sample_target_function, approximant_t{.segment = segment, .x0 = x0}, left.x, midpoint.x, right.x))
        .WillOnce(Return(residual));

    auto const actual = sut(sample_target_function, subdomain_t{left, midpoint, right, log2_width});

    EXPECT_EQ(expected, actual);
}

} // namespace interval_factory_tests

} // namespace
} // namespace crv::spline
