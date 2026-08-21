// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "tangent_extender.hpp"
#include <crv/math/float_extraction.hpp>
#include <crv/math/polynomial.hpp>
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
        constexpr auto width() const noexcept -> x_t { return right_x - left_x; }
    };

    struct interval_t
    {
        using segment_t = spline_tangent_extender_test_t::segment_t;
        cubic_t<scalar_t> cubic;
        segment_t segment;
        subdomain_t subdomain;
    };

    using sut_t = tangent_extender_t<interval_t, extended_tangent_t, float_extractor_t<scalar_t>>;
    sut_t sut{.y_limit = 100.0, .extract_float = {}};

    auto make_interval(cubic_t<scalar_t> cubic, y_t endpoint = y_t{9}) -> interval_t
    {
        auto const subdomain = subdomain_t{};
        EXPECT_CALL(mock_segment, call(subdomain.right_x, subdomain.left_x)).WillOnce(Return(endpoint));
        return {.cubic = cubic, .segment = {&mock_segment}, .subdomain = subdomain};
    }
};

TEST_F(spline_tangent_extender_test_t, derives_positive_gain_slope_and_clamp_from_transfer_cubic)
{
    // at X=5 (u=2): T=45, T'=14, G=9, so G'=(14-9)/5=1
    auto const actual = sut(make_interval({0.0, 0.0, 14.0, 17.0}));

    EXPECT_EQ(actual.y0, y_t{9});
    EXPECT_EQ(actual(x_t{1}), y_t{10});
    EXPECT_EQ(actual.x_max_delta, x_t{91});
    EXPECT_EQ(actual(x_t{92}), y_t{100});
}

TEST_F(spline_tangent_extender_test_t, supports_zero_gain_slope_as_constant_continuation)
{
    // at X=5: T=45, T'=9, G=9, so G'=0
    auto const actual = sut(make_interval({0.0, 0.0, 9.0, 27.0}));

    EXPECT_EQ(actual.y0, y_t{9});
    EXPECT_EQ(actual.slope.mantissa, 0);
    EXPECT_EQ(actual.x_max_delta, max<x_t>());
    EXPECT_EQ(actual(x_t{100}), y_t{9});
}

TEST_F(spline_tangent_extender_test_t, intercept_comes_from_fixed_segment_not_floating_transfer_endpoint)
{
    auto const actual = sut(make_interval({0.0, 0.0, 14.0, 17.0}, y_t{10}));

    EXPECT_EQ(actual.y0, y_t{10});
    EXPECT_EQ(actual(x_t{1}), y_t{11});
    EXPECT_EQ(actual.x_max_delta, x_t{90});
}

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

TEST_F(spline_tangent_extender_test_t, rejects_negative_gain_slope)
{
    // at X=5: T=45, T'=4, G=9, so G'=-1, which violates the nondecreasing authored-curve contract
    auto const interval = interval_t{.cubic = {0.0, 0.0, 4.0, 37.0}, .segment = {&mock_segment}, .subdomain = {}};
    EXPECT_DEATH(static_cast<void>(sut(interval)), "gain_slope");
}

#endif // defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

} // namespace
} // namespace crv::spline
