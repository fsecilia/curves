// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "domain_warp_curve.hpp"
#include <crv/math/jet/jet.hpp>
#include <crv/model/domain.hpp>
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
    using input_domain_t = model::input_domain_t<scalar_t>;

    struct mock_transform_t
    {
        virtual ~mock_transform_t() = default;
        MOCK_METHOD(scalar_t, scalar_apply, (scalar_t), (const, noexcept));
        MOCK_METHOD(jet_t, jet_apply, (jet_t), (const, noexcept));
        MOCK_METHOD(std::vector<scalar_t>, critical_points, (), (const));
        MOCK_METHOD(std::optional<scalar_t>, preimage_point, (scalar_t), (const, noexcept));
    };
    StrictMock<mock_transform_t> mock_transform;

    struct transform_t
    {
        mock_transform_t* mock;

        [[nodiscard]] auto preimage(input_domain_t domain) const noexcept -> input_domain_t { return domain; }

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
            return mock->preimage_point(point);
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

        mock_curve_t* mock;

        [[nodiscard]] auto operator()(scalar_t input) const noexcept -> scalar_t { return mock->scalar(input); }
        [[nodiscard]] auto operator()(jet_t input) const noexcept -> jet_t { return mock->jet(input); }
        [[nodiscard]] constexpr auto input_domain() const noexcept -> model::input_domain_t<scalar_t>
        {
            return model::input_domain_t<scalar_t>::full();
        }
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
    EXPECT_CALL(mock_transform, preimage_point(-1.0)).WillOnce(Return(std::nullopt));
    EXPECT_CALL(mock_transform, preimage_point(0.0)).WillOnce(Return(std::nullopt));
    EXPECT_CALL(mock_transform, preimage_point(3.0)).WillOnce(Return(7.0));
    EXPECT_EQ(sut.critical_points(), (std::vector<scalar_t>{2.0, 6.0, 7.0}));
}

TEST_F(shaping_domain_warp_curve_test_t, preserves_duplicate_structural_and_nested_preimage_points)
{
    EXPECT_CALL(mock_transform, critical_points()).WillOnce(Return(std::vector<scalar_t>{2.0}));
    EXPECT_CALL(mock_curve, critical_points()).WillOnce(Return(std::vector<scalar_t>{3.0}));
    EXPECT_CALL(mock_transform, preimage_point(3.0)).WillOnce(Return(2.0));
    EXPECT_EQ(sut.critical_points(), (std::vector<scalar_t>{2.0, 2.0}));
}

TEST(shaping_domain_warp_domain_test_t, stores_completed_transform_preimage)
{
    using scalar_t = float_t;
    using input_domain_t = model::input_domain_t<scalar_t>;

    struct transform_t
    {
        int_t* preimage_calls;
        input_domain_t* nested_domain;
        input_domain_t resolved;

        [[nodiscard]] auto preimage(input_domain_t domain) const noexcept -> input_domain_t
        {
            ++*preimage_calls;
            *nested_domain = domain;
            return resolved;
        }
    };

    struct curve_t
    {
        using scalar_t = crv::float_t;

        [[nodiscard]] constexpr auto operator()(scalar_t input) const noexcept -> scalar_t { return input; }
        [[nodiscard]] constexpr auto input_domain() const noexcept -> model::input_domain_t<scalar_t>
        {
            return {2.0, 7.0};
        }
        [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return {}; }
    };

    auto calls = int_t{};
    auto nested_domain = input_domain_t{};
    auto const expected = input_domain_t{-3.0, 11.0};
    auto const sut = domain_warp_curve_t<transform_t, curve_t>{{&calls, &nested_domain, expected}, {}};
    EXPECT_EQ(sut.input_domain(), expected);
    EXPECT_EQ(nested_domain, (input_domain_t{2.0, 7.0}));
    EXPECT_EQ(calls, 1);
}

struct domain_assert_curve_t
{
    using scalar_t = crv::float_t;
    using input_domain_t = model::input_domain_t<scalar_t>;

    [[nodiscard]] constexpr auto operator()(scalar_t input) const noexcept -> scalar_t { return input; }
    [[nodiscard]] constexpr auto input_domain() const noexcept -> input_domain_t { return input_domain_t::full(); }
    [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return {}; }
};

#if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

struct domain_assert_warp_transform_t
{
    using scalar_t = float_t;
    using input_domain_t = model::input_domain_t<scalar_t>;

    [[nodiscard]] constexpr auto preimage(input_domain_t) const noexcept -> input_domain_t { return {1.0, 2.0}; }

    template <typename curve_t>
    [[nodiscard]] constexpr auto apply(curve_t const& curve, scalar_t input) const noexcept -> scalar_t
    {
        return curve(input);
    }

    [[nodiscard]] auto critical_points() const -> std::vector<scalar_t> { return {}; }
    [[nodiscard]] constexpr auto try_preimage_critical_point(scalar_t) const noexcept -> std::optional<scalar_t>
    {
        return std::nullopt;
    }
};

TEST(shaping_domain_warp_domain_test_t, rejects_scalar_input_outside_stored_preimage)
{
    auto const sut = domain_warp_curve_t<domain_assert_warp_transform_t, domain_assert_curve_t>{{}, {}};
    EXPECT_DEATH(static_cast<void>(sut(0.0)), "input outside domain");
}

#endif // #if defined CRV_ENABLE_DEATH_TESTS && !defined NDEBUG

} // namespace
} // namespace crv::shaping
