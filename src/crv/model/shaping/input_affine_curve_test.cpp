// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "input_affine_curve.hpp"
#include <crv/math/jet/jet.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>
#include <optional>
#include <utility>
#include <vector>

namespace crv::shaping {
namespace {

struct shaping_input_affine_curve_test_t : Test
{
    using scalar_t = float_t;
    using jet_t = crv::jet_t<scalar_t>;

    struct domain_t
    {
        [[nodiscard]] constexpr auto contains(scalar_t) const noexcept -> bool { return true; }
    };

    struct mock_transform_t
    {
        virtual ~mock_transform_t() = default;

        MOCK_METHOD(scalar_t, scalar, (scalar_t), (const, noexcept));
        MOCK_METHOD(jet_t, jet, (jet_t), (const, noexcept));
        MOCK_METHOD(std::optional<scalar_t>, inverse, (scalar_t), (const, noexcept));
    };
    StrictMock<mock_transform_t> mock_transform;

    struct transform_t
    {
        mock_transform_t* mock;

        [[nodiscard]] auto try_apply(scalar_t input) const noexcept -> std::optional<scalar_t> { return input; }
        [[nodiscard]] auto apply(scalar_t input) const noexcept -> scalar_t { return mock->scalar(input); }
        [[nodiscard]] auto apply(jet_t input) const noexcept -> jet_t { return mock->jet(input); }
        [[nodiscard]] auto try_inverse(scalar_t input) const noexcept -> std::optional<scalar_t>
        {
            return mock->inverse(input);
        }
    };

    struct mock_curve_t
    {
        virtual ~mock_curve_t() = default;

        MOCK_METHOD(scalar_t, scalar, (scalar_t), (const, noexcept));
        MOCK_METHOD(jet_t, jet, (jet_t), (const, noexcept));
        MOCK_METHOD(std::vector<scalar_t>, critical_points, (), (const));
    };
    StrictMock<mock_curve_t> mock_curve;

    struct curve_t
    {
        using scalar_t = shaping_input_affine_curve_test_t::scalar_t;
        using domain_t = shaping_input_affine_curve_test_t::domain_t;

        mock_curve_t* mock;

        [[nodiscard]] auto operator()(scalar_t input) const noexcept -> scalar_t { return mock->scalar(input); }
        [[nodiscard]] auto operator()(jet_t input) const noexcept -> jet_t { return mock->jet(input); }
        [[nodiscard]] constexpr auto domain() const noexcept -> domain_t { return {}; }
        [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return mock->critical_points(); }
    };

    static_assert(is_curve<curve_t, scalar_t>);

    input_affine_curve_t<transform_t, curve_t> sut{transform_t{&mock_transform}, curve_t{&mock_curve}};
};

TEST_F(shaping_input_affine_curve_test_t, composes_scalar_transform_before_curve)
{
    auto const input = scalar_t{3};
    auto const transformed = scalar_t{5};
    auto const expected = scalar_t{7};
    EXPECT_CALL(mock_transform, scalar(input)).WillOnce(Return(transformed));
    EXPECT_CALL(mock_curve, scalar(transformed)).WillOnce(Return(expected));
    EXPECT_EQ(sut(input), expected);
}

TEST_F(shaping_input_affine_curve_test_t, composes_jet_transform_before_curve)
{
    auto const input = jet_t{scalar_t{3}, scalar_t{5}};
    auto const transformed = jet_t{scalar_t{7}, scalar_t{11}};
    auto const expected = jet_t{scalar_t{13}, scalar_t{17}};
    EXPECT_CALL(mock_transform, jet(input)).WillOnce(Return(transformed));
    EXPECT_CALL(mock_curve, jet(transformed)).WillOnce(Return(expected));
    EXPECT_EQ(sut(input), expected);
}

TEST_F(shaping_input_affine_curve_test_t, inverse_maps_nested_critical_points)
{
    auto const nested_points = std::vector<scalar_t>{2, 6};
    auto const expected = std::vector<scalar_t>{3, 5};
    EXPECT_CALL(mock_curve, critical_points()).WillOnce(Return(nested_points));
    EXPECT_CALL(mock_transform, inverse(scalar_t{2})).WillOnce(Return(scalar_t{3}));
    EXPECT_CALL(mock_transform, inverse(scalar_t{6})).WillOnce(Return(scalar_t{5}));
    EXPECT_EQ(sut.critical_points(), expected);
}

TEST_F(shaping_input_affine_curve_test_t, omits_unrepresentable_inverse_critical_point)
{
    EXPECT_CALL(mock_curve, critical_points()).WillOnce(Return(std::vector<scalar_t>{2}));
    EXPECT_CALL(mock_transform, inverse(scalar_t{2})).WillOnce(Return(std::nullopt));
    EXPECT_TRUE(sut.critical_points().empty());
}

struct shaping_input_affine_domain_test_t : Test
{
    using scalar_t = float_t;

    struct mock_transform_t
    {
        virtual ~mock_transform_t() = default;
        MOCK_METHOD(std::optional<scalar_t>, try_apply, (scalar_t), (const, noexcept));
    };
    StrictMock<mock_transform_t> mock_transform;

    struct transform_t
    {
        mock_transform_t* mock;

        [[nodiscard]] auto try_apply(scalar_t input) const noexcept -> std::optional<scalar_t>
        {
            return mock->try_apply(input);
        }
    };

    struct mock_domain_t
    {
        virtual ~mock_domain_t() = default;
        MOCK_METHOD(bool, contains, (scalar_t), (const, noexcept));
    };
    StrictMock<mock_domain_t> mock_domain;

    struct domain_t
    {
        mock_domain_t* mock;
        [[nodiscard]] auto contains(scalar_t input) const noexcept -> bool { return mock->contains(input); }
    };

    struct curve_t
    {
        using scalar_t = shaping_input_affine_domain_test_t::scalar_t;
        using domain_t = shaping_input_affine_domain_test_t::domain_t;

        domain_t input_domain;

        [[nodiscard]] constexpr auto operator()(scalar_t input) const noexcept -> scalar_t { return input; }
        [[nodiscard]] constexpr auto domain() const noexcept -> domain_t { return input_domain; }
        [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return {}; }
    };

    static_assert(is_curve<curve_t, scalar_t>);

    input_affine_curve_t<transform_t, curve_t> sut{transform_t{&mock_transform}, curve_t{{&mock_domain}}};
};

TEST_F(shaping_input_affine_domain_test_t, rejects_input_when_transform_is_not_representable)
{
    auto const input = scalar_t{3};
    EXPECT_CALL(mock_transform, try_apply(input)).WillOnce(Return(std::nullopt));
    EXPECT_FALSE(sut.domain().contains(input));
}

TEST_F(shaping_input_affine_domain_test_t, asks_nested_domain_about_transformed_input)
{
    auto const input = scalar_t{3};
    auto const transformed = scalar_t{5};
    EXPECT_CALL(mock_transform, try_apply(input)).WillOnce(Return(transformed));
    EXPECT_CALL(mock_domain, contains(transformed)).WillOnce(Return(true));
    EXPECT_TRUE(sut.domain().contains(input));
}

} // namespace
} // namespace crv::shaping
