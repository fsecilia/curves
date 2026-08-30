// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "domain_warp_curve.hpp"
#include <crv/math/jet/jet.hpp>
#include <crv/test/test.hpp>
#include <gmock/gmock.h>
#include <optional>
#include <utility>
#include <vector>

namespace crv::shaping {
namespace {

struct shaping_domain_warp_curve_test_t : Test
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
        MOCK_METHOD(scalar_t, scalar_apply, (scalar_t), (const, noexcept));
        MOCK_METHOD(jet_t, jet_apply, (jet_t), (const, noexcept));
        MOCK_METHOD(std::vector<scalar_t>, critical_points, (), (const));
        MOCK_METHOD(std::optional<scalar_t>, preimage, (scalar_t), (const, noexcept));
    };
    StrictMock<mock_transform_t> mock_transform;

    struct transform_t
    {
        mock_transform_t* mock;

        [[nodiscard]] auto try_apply(scalar_t input) const noexcept -> std::optional<scalar_t> { return input; }

        template <typename curve_t> [[nodiscard]] auto apply(curve_t const&, scalar_t input) const noexcept -> scalar_t
        {
            return mock->scalar_apply(input);
        }

        template <typename curve_t> [[nodiscard]] auto apply(curve_t const&, jet_t input) const noexcept -> jet_t
        {
            return mock->jet_apply(input);
        }

        [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return mock->critical_points(); }

        [[nodiscard]] auto try_preimage_critical_point(scalar_t point) const noexcept -> std::optional<scalar_t>
        {
            return mock->preimage(point);
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
        using scalar_t = shaping_domain_warp_curve_test_t::scalar_t;
        using domain_t = shaping_domain_warp_curve_test_t::domain_t;

        mock_curve_t* mock;

        [[nodiscard]] auto operator()(scalar_t input) const noexcept -> scalar_t { return mock->scalar(input); }
        [[nodiscard]] auto operator()(jet_t input) const noexcept -> jet_t { return mock->jet(input); }
        [[nodiscard]] constexpr auto domain() const noexcept -> domain_t { return {}; }
        [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return mock->critical_points(); }
    };

    static_assert(is_curve<curve_t, scalar_t>);

    domain_warp_curve_t<transform_t, curve_t> sut{transform_t{&mock_transform}, curve_t{&mock_curve}};
};

TEST_F(shaping_domain_warp_curve_test_t, delegates_scalar_composition_to_transform)
{
    EXPECT_CALL(mock_transform, scalar_apply(scalar_t{3})).WillOnce(Return(scalar_t{7}));
    EXPECT_EQ(sut(scalar_t{3}), scalar_t{7});
}

TEST_F(shaping_domain_warp_curve_test_t, delegates_jet_composition_to_transform)
{
    auto const input = jet_t{3.0, 5.0};
    auto const expected = jet_t{7.0, 11.0};
    EXPECT_CALL(mock_transform, jet_apply(input)).WillOnce(Return(expected));
    EXPECT_EQ(sut(input), expected);
}

TEST_F(shaping_domain_warp_curve_test_t, appends_reachable_nested_preimages_after_structural_points)
{
    EXPECT_CALL(mock_transform, critical_points()).WillOnce(Return(std::vector<scalar_t>{2.0, 6.0}));
    EXPECT_CALL(mock_curve, critical_points()).WillOnce(Return(std::vector<scalar_t>{-1.0, 0.0, 3.0}));
    EXPECT_CALL(mock_transform, preimage(-1.0)).WillOnce(Return(std::nullopt));
    EXPECT_CALL(mock_transform, preimage(0.0)).WillOnce(Return(std::nullopt));
    EXPECT_CALL(mock_transform, preimage(3.0)).WillOnce(Return(7.0));
    EXPECT_EQ(sut.critical_points(), (std::vector<scalar_t>{2.0, 6.0, 7.0}));
}

TEST_F(shaping_domain_warp_curve_test_t, preserves_duplicate_structural_and_nested_preimage_points)
{
    EXPECT_CALL(mock_transform, critical_points()).WillOnce(Return(std::vector<scalar_t>{2.0}));
    EXPECT_CALL(mock_curve, critical_points()).WillOnce(Return(std::vector<scalar_t>{3.0}));
    EXPECT_CALL(mock_transform, preimage(3.0)).WillOnce(Return(2.0));
    EXPECT_EQ(sut.critical_points(), (std::vector<scalar_t>{2.0, 2.0}));
}

struct shaping_domain_warp_domain_test_t : Test
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
        using scalar_t = shaping_domain_warp_domain_test_t::scalar_t;
        using domain_t = shaping_domain_warp_domain_test_t::domain_t;

        domain_t input_domain;

        [[nodiscard]] constexpr auto operator()(scalar_t input) const noexcept -> scalar_t { return input; }
        [[nodiscard]] constexpr auto domain() const noexcept -> domain_t { return input_domain; }
        [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return {}; }
    };

    static_assert(is_curve<curve_t, scalar_t>);

    domain_warp_curve_t<transform_t, curve_t> sut{transform_t{&mock_transform}, curve_t{{&mock_domain}}};
};

TEST_F(shaping_domain_warp_domain_test_t, rejects_input_when_transform_mapping_is_invalid)
{
    EXPECT_CALL(mock_transform, try_apply(scalar_t{3})).WillOnce(Return(std::nullopt));
    EXPECT_FALSE(sut.domain().contains(scalar_t{3}));
}

TEST_F(shaping_domain_warp_domain_test_t, rejects_input_when_mapped_coordinate_is_outside_nested_domain)
{
    EXPECT_CALL(mock_transform, try_apply(scalar_t{3})).WillOnce(Return(scalar_t{5}));
    EXPECT_CALL(mock_domain, contains(scalar_t{5})).WillOnce(Return(false));
    EXPECT_FALSE(sut.domain().contains(scalar_t{3}));
}

TEST_F(shaping_domain_warp_domain_test_t, accepts_input_when_mapped_coordinate_is_inside_nested_domain)
{
    EXPECT_CALL(mock_transform, try_apply(scalar_t{3})).WillOnce(Return(scalar_t{5}));
    EXPECT_CALL(mock_domain, contains(scalar_t{5})).WillOnce(Return(true));
    EXPECT_TRUE(sut.domain().contains(scalar_t{3}));
}

} // namespace
} // namespace crv::shaping
