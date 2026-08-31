// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "limited_curve.hpp"
#include <crv/math/jet/jet.hpp>
#include <crv/test/test.hpp>
#include <concepts>
#include <gmock/gmock.h>

namespace crv::shaping {
namespace {

struct shaping_limited_curve_test_t : Test
{
    using scalar_t = float_t;
    using jet_t = crv::jet_t<scalar_t>;

    struct curve_t
    {
        using scalar_t = shaping_limited_curve_test_t::scalar_t;

        scalar_t marker;
    };

    struct mock_limiter_t
    {
        virtual ~mock_limiter_t() = default;

        MOCK_METHOD(scalar_t, scalar, (scalar_t curve_marker, scalar_t input), (const, noexcept));
        MOCK_METHOD(jet_t, jet, (scalar_t curve_marker, jet_t input), (const, noexcept));
    };
    StrictMock<mock_limiter_t> mock_limiter;

    struct limiter_t
    {
        mock_limiter_t* mock;

        auto apply(curve_t const& curve, scalar_t input) const noexcept -> scalar_t
        {
            return mock->scalar(curve.marker, input);
        }

        auto apply(curve_t const& curve, jet_t input) const noexcept -> jet_t { return mock->jet(curve.marker, input); }
    };

    using sut_t = limited_curve_t<limiter_t, curve_t>;
    static_assert(std::same_as<sut_t::scalar_t, scalar_t>);

    static constexpr auto marker = scalar_t{7};
    sut_t sut{limiter_t{&mock_limiter}, curve_t{marker}};
};

TEST_F(shaping_limited_curve_test_t, forwards_scalar_composition_to_limiter)
{
    auto const input = scalar_t{3};
    auto const expected = scalar_t{11};
    EXPECT_CALL(mock_limiter, scalar(marker, input)).WillOnce(Return(expected));
    EXPECT_EQ(sut(input), expected);
}

TEST_F(shaping_limited_curve_test_t, forwards_jet_composition_to_limiter)
{
    auto const input = jet_t{scalar_t{3}, scalar_t{5}};
    auto const expected = jet_t{scalar_t{11}, scalar_t{13}};
    EXPECT_CALL(mock_limiter, jet(marker, input)).WillOnce(Return(expected));
    EXPECT_EQ(sut(input), expected);
}

} // namespace
} // namespace crv::shaping
