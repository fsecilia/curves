// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "antiderivative.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::quadrature {
namespace {

struct quadrature_antiderivative_test_t : Test
{
    using real_t = float_t;

    struct mock_integrand_t
    {
        MOCK_METHOD(real_t, call, (real_t location), (const, noexcept));
        virtual ~mock_integrand_t() = default;
    };
    StrictMock<mock_integrand_t> mock_integrand;

    struct integrand_t
    {
        mock_integrand_t* mock = nullptr;

        auto operator()(real_t location) const noexcept -> real_t { return mock->call(location); }
    };

    struct mock_rule_t
    {
        MOCK_METHOD(real_t, call, (real_t left, real_t right, integrand_t const& integrand), (const, noexcept));
        virtual ~mock_rule_t() = default;
    };
    StrictMock<mock_rule_t> mock_rule;

    struct rule_t
    {
        mock_rule_t* mock = nullptr;

        auto operator()(real_t left, real_t right, integrand_t const& integrand) const noexcept -> real_t
        {
            return mock->call(left, right, integrand);
        }
    };

    struct mock_cache_t
    {
        struct result_t
        {
            real_t left_bound;
            real_t base_integral;
        };

        MOCK_METHOD(result_t, interval, (real_t location), (const, noexcept));
        virtual ~mock_cache_t() = default;
    };
    StrictMock<mock_cache_t> mock_cache;

    struct cache_t
    {
        using result_t = mock_cache_t::result_t;

        mock_cache_t* mock = nullptr;

        auto interval(real_t location) const noexcept -> result_t { return mock->interval(location); }
    };

    using jet_t = jet_t<real_t>;

    using sut_t = antiderivative_t<real_t, std::reference_wrapper<integrand_t>, rule_t, cache_t>;

    integrand_t integrand{&mock_integrand};
    sut_t       sut{std::ref(integrand), rule_t{&mock_rule}, cache_t{&mock_cache}};
};

TEST_F(quadrature_antiderivative_test_t, call)
{
    constexpr auto expected_location      = 3.71;
    constexpr auto expected_left_bound    = 1.32;
    constexpr auto expected_base_integral = 5.27;
    constexpr auto expected_residual      = 4.15;
    constexpr auto expected_tangent       = 6.08;

    EXPECT_CALL(mock_cache, interval(expected_location))
        .WillOnce(Return(cache_t::result_t{expected_left_bound, expected_base_integral}));
    EXPECT_CALL(mock_rule, call(expected_left_bound, expected_location, Ref(integrand)))
        .WillOnce(Return(expected_residual));
    EXPECT_CALL(mock_integrand, call(expected_location)).WillOnce(Return(expected_tangent));
    auto const expected = jet_t{expected_base_integral + expected_residual, expected_tangent};

    auto const actual = sut(expected_location);

    EXPECT_EQ(expected, actual);
}

} // namespace
} // namespace crv::quadrature
