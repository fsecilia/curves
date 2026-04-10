// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "integral.hpp"
#include <crv/test/test.hpp>
#include <gmock/gmock.h>

namespace crv::quadrature {
namespace {

using real_t = float_t;

// --------------------------------------------------------------------------------------------------------------------
// compile-time tests
// --------------------------------------------------------------------------------------------------------------------

namespace compile_time_tests {

constexpr auto integrand = [](auto position) { return position * 2.0; };

struct rule_t
{
    struct estimate_t
    {
        real_t sum;
        real_t error;

        constexpr auto operator==(estimate_t const&) const noexcept -> bool = default;
    };

    constexpr auto estimate(auto left, auto right, auto const& integrand) const noexcept -> estimate_t
    {
        return {integrate(left, right, integrand), 3.5};
    }

    constexpr auto integrate(auto left, auto right, auto const& integrand) const noexcept -> real_t
    {
        return integrand(left) + integrand(right);
    }
};
constexpr auto rule = rule_t{};

constexpr auto compile_time_sut = integral_t{integrand, rule};

static_assert(compile_time_sut.estimate(3.0, 5.0) == rule_t::estimate_t{16.0, 3.5});
static_assert(compile_time_sut.integrate(3.0, 5.0) == 16.0);
static_assert(compile_time_sut.evaluate_integrand(3.0) == 6.0);

} // namespace compile_time_tests

// --------------------------------------------------------------------------------------------------------------------
// runtime tests
// --------------------------------------------------------------------------------------------------------------------

namespace runtime_tests {

struct quadrature_integral_test_t : Test
{
    struct mock_integrand_t
    {
        MOCK_METHOD(real_t, call, (real_t), (const, noexcept));
        virtual ~mock_integrand_t() = default;
    };
    StrictMock<mock_integrand_t> mock_integrand;

    struct integrand_t
    {
        mock_integrand_t* mock = nullptr;

        auto operator()(real_t position) const noexcept -> real_t { return mock->call(position); }
    };

    struct estimate_t
    {
        real_t sum;
        real_t error;

        constexpr auto operator==(estimate_t const&) const noexcept -> bool = default;
    };

    struct mock_rule_t
    {
        MOCK_METHOD(estimate_t, estimate, (real_t left, real_t right, mock_integrand_t const& integrand),
                    (const, noexcept));
        MOCK_METHOD(real_t, integrate, (real_t left, real_t right, mock_integrand_t const& integrand),
                    (const, noexcept));

        virtual ~mock_rule_t() = default;
    };
    StrictMock<mock_rule_t> mock_rule;

    struct rule_t
    {
        using estimate_t = estimate_t;

        mock_rule_t* mock = nullptr;

        auto estimate(real_t left, real_t right, integrand_t const& integrand) const noexcept -> estimate_t
        {
            return mock->estimate(left, right, *integrand.mock);
        }

        auto integrate(real_t left, real_t right, integrand_t const& integrand) const noexcept -> real_t
        {
            return mock->integrate(left, right, *integrand.mock);
        }
    };

    using sut_t = integral_t<integrand_t, rule_t>;
    sut_t sut{integrand_t{&mock_integrand}, rule_t{&mock_rule}};
};

TEST_F(quadrature_integral_test_t, estimate_delegates_to_rule)
{
    auto const left     = 3.0;
    auto const right    = 5.0;
    auto const expected = estimate_t{.sum = 7.0, .error = 11.0};
    EXPECT_CALL(mock_rule, estimate(left, right, Ref(mock_integrand))).WillOnce(Return(expected));

    auto const actual = sut.estimate(left, right);

    EXPECT_EQ(expected, actual);
}

TEST_F(quadrature_integral_test_t, integrate_delegates_to_rule)
{
    auto const left     = 3.0;
    auto const right    = 5.0;
    auto const expected = 7.0;
    EXPECT_CALL(mock_rule, integrate(left, right, Ref(mock_integrand))).WillOnce(Return(expected));

    auto const actual = sut.integrate(left, right);

    EXPECT_EQ(expected, actual);
}

TEST_F(quadrature_integral_test_t, evaluate_integral_delegates_to_integrand)
{
    auto const position = 3.0;
    auto const expected = 5.0;
    EXPECT_CALL(mock_integrand, call(position)).WillOnce(Return(expected));

    auto const actual = sut.evaluate_integrand(position);

    EXPECT_EQ(expected, actual);
}

} // namespace runtime_tests

} // namespace
} // namespace crv::quadrature
