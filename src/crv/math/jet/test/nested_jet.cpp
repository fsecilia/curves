// SPDX-License-Identifier: MIT
/**
    \file
    \brief tests jet composition over jets

    These tests verify that autodiff composes correctly to calc second derivatives via jet_t<jet_t<double>>.

    \copyright Copyright (C) 2026 Frank Secilia
*/

#include <crv/math/jet/jet.hpp>
#include <crv/test/test.hpp>

namespace crv {
namespace {

struct nested_jet_test_t : Test
{
    using scalar_t = double;
    using value_t  = jet_t<scalar_t>;
    using sut_t    = jet_t<value_t>;

    // seed with primish numbers
    static constexpr scalar_t s{1.3};
    static constexpr value_t  v{1.7, 1.9};
    static constexpr sut_t    x{{2.3, 3.1}, {5.3, 7.1}};
    static constexpr sut_t    y{{5.9, 7.3}, {8.3, 9.7}};

    static constexpr scalar_t eps = 1e-10;

    constexpr auto compare(sut_t const& expected, sut_t const& actual, scalar_t tolerance = eps) noexcept -> void
    {
        EXPECT_NEAR(expected.f.f, actual.f.f, eps);
        EXPECT_NEAR(expected.f.df, actual.f.df, eps);
        EXPECT_NEAR(expected.df.f, actual.df.f, eps);
        EXPECT_NEAR(expected.df.df, actual.df.df, eps);
    }
};

// --------------------------------------------------------------------------------------------------------------------
// Arithmetic
// --------------------------------------------------------------------------------------------------------------------

struct nested_jet_test_arithmetic_t : nested_jet_test_t
{};

TEST_F(nested_jet_test_arithmetic_t, compound_plus_scalar)
{
    auto const expected = sut_t{{x.f.f + s, x.f.df}, x.df};

    auto        sut    = x;
    auto const& actual = sut += s;

    ASSERT_EQ(&sut, &actual);
    ASSERT_EQ(expected, actual);
}

TEST_F(nested_jet_test_arithmetic_t, scalar_plus_jet)
{
    auto const expected = sut_t{{x.f.f + s, x.f.df}, x.df};

    auto const actual = x + s;

    ASSERT_EQ(expected, actual);
}

TEST_F(nested_jet_test_arithmetic_t, jet_plus_scalar)
{
    auto const expected = sut_t{{s + x.f.f, x.f.df}, x.df};

    auto const actual = s + x;

    ASSERT_EQ(expected, actual);
}

TEST_F(nested_jet_test_arithmetic_t, compound_minus_scalar)
{
    auto const expected = sut_t{{x.f.f - s, x.f.df}, x.df};

    auto        sut    = x;
    auto const& actual = sut -= s;

    ASSERT_EQ(&sut, &actual);
    ASSERT_EQ(expected, actual);
}

TEST_F(nested_jet_test_arithmetic_t, scalar_minus_jet)
{
    auto const expected = sut_t{{x.f.f - s, x.f.df}, x.df};

    auto const actual = x - s;

    ASSERT_EQ(expected, actual);
}

TEST_F(nested_jet_test_arithmetic_t, jet_minus_scalar)
{
    auto const expected = sut_t{{s - x.f.f, -x.f.df}, -x.df};

    auto const actual = s - x;

    ASSERT_EQ(expected, actual);
}

TEST_F(nested_jet_test_arithmetic_t, compound_times_scalar)
{
    auto const expected = sut_t{x.f * s, x.df * s};

    auto        sut    = x;
    auto const& actual = sut *= s;

    ASSERT_EQ(&sut, &actual);
    ASSERT_EQ(expected, actual);
}

TEST_F(nested_jet_test_arithmetic_t, scalar_times_jet)
{
    auto const expected = sut_t{x.f * s, x.df * s};

    auto const actual = x * s;

    ASSERT_EQ(expected, actual);
}

TEST_F(nested_jet_test_arithmetic_t, jet_times_scalar)
{
    auto const expected = sut_t{x.f * s, x.df * s};

    auto const actual = s * x;

    ASSERT_EQ(expected, actual);
}

TEST_F(nested_jet_test_arithmetic_t, compound_over_scalar)
{
    auto const expected = sut_t{x.f / s, x.df / s};

    auto        sut    = x;
    auto const& actual = sut /= s;

    ASSERT_EQ(&sut, &actual);
    ASSERT_EQ(expected, actual);
}

TEST_F(nested_jet_test_arithmetic_t, scalar_over_jet)
{
    auto const expected = sut_t{x.f / s, x.df / s};

    auto const actual = x / s;

    ASSERT_EQ(expected, actual);
}

TEST_F(nested_jet_test_arithmetic_t, jet_over_scalar)
{
    auto const expected = sut_t{s / x.f, -s * x.df / (x.f * x.f)};

    auto const actual = s / x;

    compare(expected, actual);
}

} // namespace
} // namespace crv
