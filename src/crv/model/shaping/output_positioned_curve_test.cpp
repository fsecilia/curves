// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "output_positioned_curve.hpp"
#include <crv/math/jet/jet.hpp>
#include <crv/model/domain.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>
#include <vector>

namespace crv::shaping {
namespace {

struct output_positioned_curve_test_t : Test
{
    using scalar_t = float_t;
    using jet_t = crv::jet_t<scalar_t>;

    using input_domain_t = model::input_domain_t<scalar_t>;

    struct curve_mock_t
    {
        virtual ~curve_mock_t() = default;
        MOCK_METHOD(scalar_t, scalar, (scalar_t), (const, noexcept));
        MOCK_METHOD(jet_t, jet, (jet_t), (const, noexcept));
        MOCK_METHOD(std::vector<scalar_t>, critical_points, (), (const));
    };
    StrictMock<curve_mock_t> curve_mock;

    struct curve_t
    {
        using scalar_t = output_positioned_curve_test_t::scalar_t;
        curve_mock_t* mock;
        input_domain_t domain;

        [[nodiscard]] auto operator()(scalar_t input) const noexcept -> scalar_t { return mock->scalar(input); }
        [[nodiscard]] auto operator()(jet_t input) const noexcept -> jet_t { return mock->jet(input); }
        [[nodiscard]] auto input_domain() const noexcept -> model::input_domain_t<scalar_t> { return domain; }
        [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return mock->critical_points(); }
    };

    struct transform_mock_t
    {
        virtual ~transform_mock_t() = default;
        MOCK_METHOD(scalar_t, scalar, (scalar_t), (const, noexcept));
        MOCK_METHOD(jet_t, jet, (jet_t), (const, noexcept));
    };
    StrictMock<transform_mock_t> transform_mock;

    struct transform_t
    {
        transform_mock_t* mock;
        [[nodiscard]] auto apply(scalar_t output) const noexcept -> scalar_t { return mock->scalar(output); }
        [[nodiscard]] auto apply(jet_t output) const noexcept -> jet_t { return mock->jet(output); }
    };

    static_assert(is_curve<curve_t, scalar_t>);

    static_assert(is_curve<output_positioned_curve_t<transform_t, curve_t>, scalar_t>);

    output_positioned_curve_t<transform_t, curve_t> sut{{&transform_mock}, {&curve_mock, {13.0, 17.0}}};
};

TEST_F(output_positioned_curve_test_t, positions_nested_scalar_output)
{
    EXPECT_CALL(curve_mock, scalar(3.0)).WillOnce(Return(5.0));
    EXPECT_CALL(transform_mock, scalar(5.0)).WillOnce(Return(7.0));
    EXPECT_EQ(sut(3.0), 7.0);
}

TEST_F(output_positioned_curve_test_t, positions_nested_jet_output)
{
    auto const input = jet_t{3.0, 5.0};
    auto const nested = jet_t{7.0, 11.0};
    auto const expected = jet_t{13.0, 17.0};
    EXPECT_CALL(curve_mock, jet(input)).WillOnce(Return(nested));
    EXPECT_CALL(transform_mock, jet(nested)).WillOnce(Return(expected));
    EXPECT_EQ(sut(input), expected);
}

TEST_F(output_positioned_curve_test_t, forwards_nested_domain)
{
    EXPECT_EQ(sut.input_domain(), (input_domain_t{13.0, 17.0}));
}

TEST_F(output_positioned_curve_test_t, forwards_nested_critical_points)
{
    auto const points = std::vector<scalar_t>{2.0, 5.0};
    EXPECT_CALL(curve_mock, critical_points()).WillOnce(Return(points));
    EXPECT_EQ(sut.critical_points(), points);
}

} // namespace
} // namespace crv::shaping
