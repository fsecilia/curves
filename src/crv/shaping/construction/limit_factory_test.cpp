// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "limit_factory.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::shaping::construction {
namespace {

struct shaping_construction_limit_factory_test_t : Test
{
    using real_t = float_t;

    struct mock_transition_t
    {
        virtual ~mock_transition_t() = default;
        MOCK_METHOD(real_t, call, (real_t), (const, noexcept));
    };
    StrictMock<mock_transition_t> mock_transition;

    struct transition_t
    {
        mock_transition_t* mock = nullptr;
        auto operator()(real_t t) const noexcept { return mock->call(t); }

        // compares pointers shallowly, by value
        auto operator==(transition_t const&) const noexcept -> bool = default;
    };

    struct mock_curve_t
    {
        virtual ~mock_curve_t() = default;
        MOCK_METHOD(real_t, call, (real_t), (const, noexcept));
        auto operator()(real_t x) const noexcept -> real_t { return call(x); }
    };
    using curve_t = StrictMock<mock_curve_t>;
    curve_t mock_curve;

    struct limit_t
    {
        real_t transition_start;
        real_t transition_width;
        real_t min_transition_width;
        real_t blend;
        transition_t transition;

        constexpr auto operator==(limit_t const&) const noexcept -> bool = default;
    };

    struct mock_locator_t
    {
        virtual ~mock_locator_t() = default;
        MOCK_METHOD(real_t, call,
            (real_t input_min, real_t input_max, real_t transition_width, real_t transition_max, real_t output_limit,
                curve_t const& curve),
            (const, noexcept));
    };
    StrictMock<mock_locator_t> mock_locator;

    struct locator_t
    {
        mock_locator_t* mock = nullptr;
        auto operator()(real_t input_min, real_t input_max, real_t transition_width, real_t transition_max,
            real_t output_limit, curve_t const& curve) const noexcept -> real_t
        {
            return mock->call(input_min, input_max, transition_width, transition_max, output_limit, curve);
        }
    };

    struct mock_engager_t
    {
        virtual ~mock_engager_t() = default;
        MOCK_METHOD(real_t, call, (real_t, real_t, real_t), (const, noexcept));
    };
    StrictMock<mock_engager_t> mock_engager;

    struct engager_t
    {
        mock_engager_t* mock = nullptr;
        auto operator()(real_t output_max, real_t output_limit, real_t transition_height) const noexcept -> real_t
        {
            return mock->call(output_max, output_limit, transition_height);
        }
    };

    using sut_t = limit_factory_t<real_t, limit_t, transition_t, locator_t, engager_t>;
    sut_t sut{locator_t{&mock_locator}, engager_t{&mock_engager}};
};

TEST_F(shaping_construction_limit_factory_test_t, build)
{
    auto const input_min = 1.0;
    auto const input_max = 10.0;
    auto const transition_width = 2.0;
    auto const min_transition_width = 0.125;
    auto const transition_max = 3.0;
    auto const output_limit = 100.0;

    auto const transition_start = 5.0;
    auto const output_max = 50.0;
    auto const transition_height = 0.5;
    auto const blend = 0.25;

    EXPECT_CALL(mock_transition, call(1.0)).WillOnce(Return(transition_max));
    EXPECT_CALL(
        mock_locator, call(input_min, input_max, transition_width, transition_max, output_limit, Ref(mock_curve)))
        .WillOnce(Return(transition_start));
    EXPECT_CALL(mock_curve, call(input_max)).WillOnce(Return(output_max));
    EXPECT_CALL(mock_engager, call(output_max, output_limit, transition_height)).WillOnce(Return(blend));

    auto const expected
        = limit_t{transition_start, transition_width, min_transition_width, blend, transition_t{&mock_transition}};
    auto const actual = sut(input_min, input_max, transition_width, transition_height, min_transition_width,
        output_limit, transition_t{&mock_transition}, mock_curve);

    EXPECT_EQ(expected, actual);
}

} // namespace
} // namespace crv::shaping::construction
