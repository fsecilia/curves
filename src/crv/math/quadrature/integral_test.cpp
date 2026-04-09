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
constexpr auto rule      = [](auto left, auto right, auto integrand) {
    return rule_result_t<decltype(left)>{integrand(left) + integrand(right), 0.0};
};

constexpr auto compile_time_sut = integral_t{integrand, rule};

static_assert(compile_time_sut(3.0, 5.0).value == 16.0);
static_assert(compile_time_sut(3.0) == 6.0);

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

    struct mock_rule_t
    {
        MOCK_METHOD(rule_result_t<real_t>, call, (real_t left, real_t right, mock_integrand_t const& integrand),
                    (const, noexcept));
        virtual ~mock_rule_t() = default;
    };
    StrictMock<mock_rule_t> mock_rule;

    struct rule_t
    {
        mock_rule_t* mock = nullptr;

        auto operator()(real_t left, real_t right, integrand_t const& integrand) const noexcept -> rule_result_t<real_t>
        {
            return mock->call(left, right, *integrand.mock);
        }
    };

    using sut_t = integral_t<integrand_t, rule_t>;
    sut_t sut{integrand_t{&mock_integrand}, rule_t{&mock_rule}};
};

TEST_F(quadrature_integral_test_t, integrate_delegates_to_rule)
{
    auto const left     = 3.0;
    auto const right    = 5.0;
    auto const expected = rule_result_t<real_t>{.value = 7.0, .error = 11.0};
    EXPECT_CALL(mock_rule, call(left, right, Ref(mock_integrand))).WillOnce(Return(expected));

    auto const actual = sut(left, right);

    EXPECT_EQ(expected, actual);
}

TEST_F(quadrature_integral_test_t, evaluate_delegates_to_integrand)
{
    auto const position = 3.0;
    auto const expected = 5.0;
    EXPECT_CALL(mock_integrand, call(position)).WillOnce(Return(expected));

    auto const actual = sut(position);

    EXPECT_EQ(expected, actual);
}

} // namespace runtime_tests

} // namespace
} // namespace crv::quadrature
