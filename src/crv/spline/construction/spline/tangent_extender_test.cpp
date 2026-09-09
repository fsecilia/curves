// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "tangent_extender.hpp"
#include <crv/math/float_extraction.hpp>
#include <crv/math/jet/jet.hpp>
#include <crv/spline/segment.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::spline {
namespace {

struct spline_tangent_extender_test_t : Test
{
    using scalar_t = float_t;
    using x_t = fixed_t<int64_t, 14>;
    using y_t = fixed_t<int64_t, 25>;
    using unpacked_field_t = spline::unpacked_field_t<int64_t>;
    using extended_tangent_t = spline::extended_tangent_t<x_t, y_t, unpacked_field_t>;

    static constexpr auto left_x = x_t{3};
    static constexpr auto right_x = x_t{5};

    struct mock_segment_t
    {
        virtual ~mock_segment_t() = default;
        MOCK_METHOD(y_t, call, (x_t, x_t), (const, noexcept));
    };
    StrictMock<mock_segment_t> mock_segment;

    struct segment_t
    {
        using x_t = spline_tangent_extender_test_t::x_t;
        mock_segment_t* mock = nullptr;

        auto operator()(x_t x, x_t x0) const noexcept -> y_t { return mock->call(x, x0); }
    };

    struct subdomain_t
    {
        x_t left_x = spline_tangent_extender_test_t::left_x;
        x_t right_x = spline_tangent_extender_test_t::right_x;
    };

    struct interval_t
    {
        using segment_t = spline_tangent_extender_test_t::segment_t;
        segment_t segment;
        scalar_t right_gain_slope;
        subdomain_t subdomain;
    };

    using sut_t = tangent_extender_t<interval_t, extended_tangent_t, float_extractor_t<scalar_t>>;
    sut_t sut{.y_limit = 100.0, .extract_float = {}};

    auto make_interval(scalar_t right_gain_slope, y_t endpoint = y_t{9}, x_t left = left_x, x_t right = right_x)
        -> interval_t
    {
        auto const subdomain = subdomain_t{.left_x = left, .right_x = right};
        EXPECT_CALL(mock_segment, call(subdomain.right_x, subdomain.left_x)).WillOnce(Return(endpoint));
        return {.segment = {&mock_segment}, .right_gain_slope = right_gain_slope, .subdomain = subdomain};
    }

    struct contradictory_subdomain_t
    {
        x_t left_x = spline_tangent_extender_test_t::left_x;
        x_t right_x = spline_tangent_extender_test_t::right_x;
        struct
        {
            jet_t<scalar_t> y;
        } right;
    };

    struct contradictory_interval_t
    {
        using segment_t = spline_tangent_extender_test_t::segment_t;
        segment_t segment;
        scalar_t right_gain_slope;
        contradictory_subdomain_t subdomain;
    };

    using contradictory_sut_t
        = tangent_extender_t<contradictory_interval_t, extended_tangent_t, float_extractor_t<scalar_t>>;
    contradictory_sut_t contradictory_sut{.y_limit = 100.0, .extract_float = {}};

    auto make_contradictory_interval(scalar_t right_gain_slope, jet_t<scalar_t> transfer_endpoint,
        y_t endpoint = y_t{9}, x_t left = left_x, x_t right = right_x) -> contradictory_interval_t
    {
        auto const subdomain
            = contradictory_subdomain_t{.left_x = left, .right_x = right, .right = {.y = transfer_endpoint}};
        EXPECT_CALL(mock_segment, call(subdomain.right_x, subdomain.left_x)).WillOnce(Return(endpoint));
        return {.segment = {&mock_segment}, .right_gain_slope = right_gain_slope, .subdomain = subdomain};
    }
};

TEST_F(spline_tangent_extender_test_t, uses_positive_represented_gain_slope_and_clamps_to_limit)
{
    auto const actual = sut(make_interval(1.0));

    EXPECT_EQ(actual.y0, y_t{9});
    EXPECT_EQ(actual(x_t{1}), y_t{10});
    EXPECT_EQ(actual.x_max_delta, x_t{91});
    EXPECT_EQ(actual(x_t{92}), y_t{100});
}

TEST_F(spline_tangent_extender_test_t, supports_zero_gain_slope_as_constant_continuation)
{
    auto const actual = sut(make_interval(0.0));

    EXPECT_EQ(actual.y0, y_t{9});
    EXPECT_EQ(actual.slope.significand, 0);
    EXPECT_EQ(actual.x_max_delta, max<x_t>());
    EXPECT_EQ(actual(x_t{100}), y_t{9});
}

TEST_F(spline_tangent_extender_test_t, anchor_comes_from_packed_segment)
{
    auto const actual = sut(make_interval(1.0, y_t{10}));

    EXPECT_EQ(actual.y0, y_t{10});
    EXPECT_EQ(actual(x_t{1}), y_t{11});
    EXPECT_EQ(actual.x_max_delta, x_t{90});
}

TEST_F(spline_tangent_extender_test_t, represented_slope_beats_contradictory_target_endpoint)
{
    // target endpoint implies G'=-1 at X=5; accepted represented slope is authoritative.
    auto const actual = contradictory_sut(make_contradictory_interval(0.0, {45.0, 4.0}));

    EXPECT_EQ(actual.slope.significand, 0);
    EXPECT_EQ(actual.x_max_delta, max<x_t>());
    EXPECT_EQ(actual(x_t{100}), y_t{9});
}

// regression test: historical target reconstruction could perturb a mathematically constant final gain slope.
TEST_F(spline_tangent_extender_test_t, represented_constant_gain_receipt_avoids_zero_slope_cancellation)
{
    auto constexpr gain = scalar_t{12.989};
    auto constexpr x_max = scalar_t{256};
    auto const actual
        = contradictory_sut(make_contradictory_interval(0.0, {x_max * gain, gain}, y_t{13}, x_t{20}, x_t{256}));

    EXPECT_EQ(actual.slope.significand, 0);
}

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST_F(spline_tangent_extender_test_t, rejects_negative_gain_slope)
{
    auto const interval = interval_t{.segment = {&mock_segment}, .right_gain_slope = -1.0, .subdomain = {}};
    EXPECT_DEATH(static_cast<void>(sut(interval)), "gain_slope");
}

#endif // defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

} // namespace
} // namespace crv::spline
