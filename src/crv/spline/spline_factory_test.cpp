// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "spline_factory.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::spline {
namespace {

struct spline_factory_test_t : Test
{
    struct mock_generator_t
    {
        using result_t = int_t;

        virtual ~mock_generator_t() = default;
        MOCK_METHOD(result_t, call, (std::vector<float_t> const&), ());
    };
    StrictMock<mock_generator_t> mock_generator;

    struct generator_t
    {
        using result_t = mock_generator_t::result_t;

        mock_generator_t* mock = nullptr;

        template <typename spline_t, typename target_function_t, typename critical_points_t>
        auto operator()(spline_t&, target_function_t&&, critical_points_t&& points) const -> result_t
        {
            return mock->call(points);
        }
    };

    struct mock_generator_factory_t
    {
        virtual ~mock_generator_factory_t() = default;
        MOCK_METHOD(generator_t, call, (float_t), ());
    };
    StrictMock<mock_generator_factory_t> mock_factory;

    struct generator_factory_t
    {
        using product_t = generator_t;

        mock_generator_factory_t* mock = nullptr;

        auto operator()(float_t tolerance) const -> product_t { return mock->call(tolerance); }
    };

    struct policy_t
    {
        using scalar_t = float_t;
        using x_t = float_t;
        using spline_t = int_t;
    };

    using sut_t = spline_factory_t<policy_t, generator_factory_t>;
    sut_t const sut{.create_generator = generator_factory_t{&mock_factory}};
};

TEST_F(spline_factory_test_t, forwards_arguments_to_generator)
{
    auto const target_tolerance = 0.05;
    auto const target_points = std::vector<float_t>{1.0, 2.0};
    struct target_t
    {
        constexpr auto transfer(float_t x) const noexcept -> float_t { return x; }
    };
    auto const target = target_t{};
    auto spline = int_t{0};

    EXPECT_CALL(mock_factory, call(target_tolerance)).WillOnce(::testing::Return(generator_t{&mock_generator}));
    EXPECT_CALL(mock_generator, call(target_points)).WillOnce(Return(17));

    EXPECT_EQ(sut(spline, target, target_tolerance, target_points), 17);
}

} // namespace
} // namespace crv::spline
